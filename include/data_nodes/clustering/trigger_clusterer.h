#include "clusterer.h"
#include "../../devices/current_device.h"
#include <optional>
#pragma once


template <typename hit_type>
class window_frequency_state
{
    const double MIN_WINDOW_TIME_ = 50000000; //50ms
    double first_hit_toa_ = 0;
    double last_hit_toa_ = 0;
    uint64_t hit_count_ = 0;
    public:
    bool is_end() const
    {
        return last_hit_toa_ - first_hit_toa_ > MIN_WINDOW_TIME_;
    }
    void update(const hit_type & hit)
    {
        ++hit_count_;
        last_hit_toa_ = hit.toa();
    }
    void reset()
    {
        first_hit_toa_ = last_hit_toa_;
        hit_count_ = 1; 
    }
    uint64_t hit_count() const
    {
        return hit_count_;
    }
    double window_width_actual() const
    {
        return last_hit_toa_ - first_hit_toa_;
    }
    double last_hit_toa() const
    {
        return last_hit_toa_;
    }


};
template <typename hit_type>
class energy_dist_state
{
    static constexpr uint16_t ENERGY_DISTR_BIN_COUNT_ = 11; 
    static constexpr std::array<double, ENERGY_DISTR_BIN_COUNT_ - 1> ENERGY_DISTR_UPP_THL_ = {
        5., 10., 20., 30., 50., 100., 200., 400., 600., 1000.,
    };
    const double MIN_WINDOW_TIME_ = 500000;
    double first_hit_toa_ = 0;
    double last_hit_toa_ = 0;
    std::array<double, ENERGY_DISTR_BIN_COUNT_> energy_distr_;
    public:
    bool is_end()
    {
        return last_hit_toa_ - first_hit_toa_ > MIN_WINDOW_TIME_;
    }
    void update(const hit_type & hit)
    {
        for(uint16_t i = 0; i < energy_distr_.size() ; ++i)
        {
            if(hit.e() < ENERGY_DISTR_UPP_THL_[i])
            {
                ++energy_distr_[i];
                break;
            }
        }
        if(hit.e() > ENERGY_DISTR_UPP_THL_.back())
        {
            ++energy_distr_[energy_distr_.size() - 1];
        }

    }
    void reset()
    {
        first_hit_toa_ = last_hit_toa_;
        energy_distr_ = std::array<double, ENERGY_DISTR_BIN_COUNT_>();
    }
    energy_dist_state() :
    energy_distr_(std::array<double, ENERGY_DISTR_BIN_COUNT_>())
    {}
    

};



template <typename mm_hit, typename trigger_type>
class trigger_clusterer : pixel_list_clusterer<cluster>
//public i_simple_consumer<mm_hit>, public i_simple_producer<cluster<mm_hit>>
{
    protected:    
    struct unfinished_energy_cluster;
    using cluster_list = std::list<unfinished_energy_cluster>;
    using cluster_it = typename cluster_list::iterator;
    using cluster_it_list = typename std::list<cluster_it>;
    struct unfinished_energy_cluster
    {
        cluster<mm_hit> cl;
        std::vector<typename cluster_it_list::iterator> pixel_iterators;
        cluster_it self;
        bool selected = false;
        bool has_high_energy = false;
        unfinished_energy_cluster()
        {
        }
        void select()
        {
            selected = true;
        }
        void unselect()
        {
            selected = false;
        }
    };
    const double CLUSTER_CLOSING_DT = 300;   //time after which the cluster is closed (> DIFF_DT, because of delays in the datastream)
    const double CLUSTER_DIFF_DT = 300;     //time that marks the max difference of cluster last_toa()
    const uint32_t MAX_PIXEL_COUNT = 2 << 15;
    const uint32_t BUFFER_CHECK_INTERVAL = 2;
    const uint32_t WRITE_INTERVAL = 2 << 6;
    const double BUFFER_FORGET_DT = 50000000;
    const std::vector<coord> EIGHT_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1}};
    bool finished_ = false;
    std::vector<cluster_it_list> pixel_lists_; 
    cluster_list unfinished_clusters_;
    uint32_t unfinished_clusters_count_;
    using pix_matrix_type = std::array<std::array<double, current_chip::chip_type::size_y()>, current_chip::chip_type::size_x()>;
    trigger_type trigger_;
    typename trigger_type::state_type window_state_;
    protected:
    uint64_t processed_hit_count_;
    using buffer_type = std::deque<mm_hit>;
    //const uint32_t expected_buffer_size = 2 << ;
    buffer_type hit_buffer_;
    //double toa_crit_interval_end_ = 0;
    enum class clusterer_state
    {
        processing,
        monitoring
    };
    clusterer_state current_state;
    void merge_clusters(unfinished_energy_cluster & bigger_cluster, unfinished_energy_cluster & smaller_cluster) 
    {
        
        for(auto & pix_it : smaller_cluster.pixel_iterators) //update iterator
        {
            *pix_it = bigger_cluster.self; //CHECK ME if we needd more dereferences ()
        }
        
        bigger_cluster.cl.hits().reserve(bigger_cluster.cl.hits().size() + smaller_cluster.cl.hits().size());
        bigger_cluster.cl.hits().insert(bigger_cluster.cl.hits().end(), std::make_move_iterator(smaller_cluster.cl.hits().begin()),
        std::make_move_iterator(smaller_cluster.cl.hits().end())); //TODO try hits in std list and then concat in O(1)
        
        //merge clusters
        bigger_cluster.pixel_iterators.reserve(bigger_cluster.pixel_iterators.size() + smaller_cluster.pixel_iterators.size());
        bigger_cluster.pixel_iterators.insert(bigger_cluster.pixel_iterators.end(), std::make_move_iterator(smaller_cluster.pixel_iterators.begin()),
        std::make_move_iterator(smaller_cluster.pixel_iterators.end())); //TODO try hits in std list and then concat in O(1)
        
        //update first and last toa
        bigger_cluster.cl.set_first_toa(std::min(bigger_cluster.cl.first_toa(), smaller_cluster.cl.first_toa()));
        bigger_cluster.cl.set_last_toa(std::max(bigger_cluster.cl.last_toa(), smaller_cluster.cl.last_toa()));
        //update bool saying if cluster has high energy pixel and number of such clusters
        bigger_cluster.has_high_energy = bigger_cluster.has_high_energy || smaller_cluster.has_high_energy;
        unfinished_clusters_.erase(smaller_cluster.self);
        --unfinished_clusters_count_;
        
    }
    std::vector<cluster_it> find_neighboring_clusters(const coord& base_coord, double toa, cluster_it& biggest_cluster)
    {
        std::vector<cluster_it> uniq_neighbor_cluster_its;
        //uint32_t max_cluster_size = 0; 
        double min_toa = ULONG_MAX;
        for (auto neighbor_offset : EIGHT_NEIGHBORS)   //check all neighbor indexes
        {       
            if(!base_coord.is_valid_neighbor(neighbor_offset))
                continue;
            uint32_t neighbor_index = neighbor_offset.linearize() + base_coord.linearize();        
                for (auto & neighbor_cl_it : pixel_lists_[neighbor_index])  //iterate over each cluster neighbor pixel can be in
                //TODO do reverse iteration and break when finding a match - as there cannot two "already mergable" clusters on a single neighbor pixel
                {
                    if(toa - CLUSTER_DIFF_DT < neighbor_cl_it->cl.last_toa() && !neighbor_cl_it->selected) //TODO order
                    {                                                                     //which of conditions is more likely to fail?
                        neighbor_cl_it->select();
                        uniq_neighbor_cluster_its.push_back(neighbor_cl_it);
                        if(neighbor_cl_it->cl.first_toa() < min_toa)   //find biggest cluster for possible merging
                        {
                            min_toa= neighbor_cl_it->cl.first_toa();
                            biggest_cluster = neighbor_cl_it;
                        }
                        break;
                    }
                }
        }
        for (auto& neighbor_cluster_it : uniq_neighbor_cluster_its)
        {
            neighbor_cluster_it->unselect();    //unselect to prepare clusters for next neighbor search
        }
        
        return uniq_neighbor_cluster_its;    
    }
    void add_new_hit(mm_hit & hit, cluster_it & cluster_iterator)
    {
        //update cluster itself, assumes the cluster exists

        auto &  target_pixel_list = pixel_lists_[hit.coordinates().linearize()];
        
        target_pixel_list.push_front(cluster_iterator);
        cluster_iterator->pixel_iterators.push_back(target_pixel_list.begin()); //beware, we need to push in the same order,
                                                                                //as we push hits in addhit and in merging
        cluster_iterator->cl.add_hit(std::move(hit));
        //update the pixel list
    }
    bool is_old(double last_toa, const cluster<mm_hit> & cluster)
    {
        return cluster.last_toa() < last_toa - CLUSTER_CLOSING_DT; 
    }
    void write_old_clusters(double last_toa = 0)
    {
        //old clusters should be at the end of the list
        while (unfinished_clusters_count_ > 0  && (is_old(last_toa, unfinished_clusters_.back().cl) || finished_))
        {
            unfinished_energy_cluster & current = unfinished_clusters_.back();
            auto & current_hits = current.cl.hits(); 
            for(uint32_t i = 0; i < current.pixel_iterators.size(); i++) //update iterator
            {
                auto & pixel_list_row = pixel_lists_[current_hits[i].coordinates().linearize()];
                pixel_list_row.erase(current.pixel_iterators[i]); //FIXME pixel_list_rows should be appended in other direction
            }
            this->writer_.write(std::move(unfinished_clusters_.back().cl));
            unfinished_clusters_.pop_back();
            --unfinished_clusters_count_;
        }    
    }
    void clusterize_hit(mm_hit & hit) //does actual pixel_pointer_clusterer-like processing
    {
        cluster_it target_cluster = unfinished_clusters_.end();
        const auto neighboring_clusters = find_neighboring_clusters(hit.coordinates(), hit.toa(), target_cluster);
        switch(neighboring_clusters.size())
        {
            case 0:
                
                unfinished_clusters_.emplace_front(unfinished_energy_cluster{});
                unfinished_clusters_.begin()->self = unfinished_clusters_.begin();
                ++unfinished_clusters_count_;
                target_cluster = unfinished_clusters_.begin();
                break;
            case 1:
                //target cluster is already selected by find neighboring clusters         
                break;
            default:
                // we found the largest cluster and move hits from smaller clusters to the biggest one      
                for (auto & neighboring_cluster_it : neighboring_clusters)
                {   
                // TODO  merge clusters and then add pixel to it
                    if(neighboring_cluster_it != target_cluster)
                    {

                        merge_clusters(*target_cluster, *neighboring_cluster_it);
                    }
                }
                break;
        }
        add_new_hit(hit, target_cluster);
        ++processed_hit_count_;
        //std::cout << processed_hit_count_ << std::endl;
    }
    
    void wrapped_process_hit(mm_hit & hit) //used for benchmarking
    {
        if(processed_hit_count_ % WRITE_INTERVAL == 0) 
            write_old_clusters(hit.toa());
        window_state_.update(hit);
        bool end_of_window = window_state_.is_end();
        if(processed_hit_count_ % BUFFER_CHECK_INTERVAL == 0)
            forget_old_hits(hit.toa());
        
        if(current_state == clusterer_state::processing)
            process_hit(hit);
        else if(current_state == clusterer_state::monitoring)
            hit_buffer_.push_back(hit);

        if(end_of_window)
        {                                                      //and we were in forgetting phase
            if(current_state == clusterer_state::monitoring && trigger_.trigger(window_state_)) //we just found interesting hit
            {                                                      
                process_all_buffered();                           
            }  
            if(trigger_.untrigger(window_state_))                          //we written out all high energy clusters for now, 
                current_state = clusterer_state::monitoring;  //so we can start monitoring again     
            window_state_.reset();                      
        }
                                    //and process all hits retrospectively
    }
    void forget_old_hits(double current_toa) //removes old hits from buffer
    {
        while(hit_buffer_.size() > 0 && hit_buffer_.front().toa() <  current_toa - BUFFER_FORGET_DT)
        {
            hit_buffer_.pop_front();
        }
    }
    
    void process_all_buffered()
    {
        current_state = clusterer_state::processing;
        uint64_t buffer_size =  hit_buffer_.size();
        for(uint32_t i = 0; i < buffer_size; ++i)
        {
            clusterize_hit(hit_buffer_[i]);
            if(i % WRITE_INTERVAL == 0) 
                write_old_clusters(hit_buffer_[i].toa());
             //we can remove the hit from buffer, 
        }                            //as it is now located inside of a unfinished clusters data structure
        hit_buffer_.clear();
 
        //TODO when to start monitoring again ? set toa_crit_interval_end_
        //TODO try buffered printer
    }
    void write_remaining_clusters()
    {
        finished_ = true;
        write_old_clusters();
    }
    
    public:
    virtual void start() override
    {
        mm_hit hit;
        this->reader_.read(hit);
        int num_hits = 0;
        while(hit.is_valid())
        {
            wrapped_process_hit(hit);
            num_hits++;
            this->reader_.read(hit);
        }
        std::cout << "++++++++++" << std::endl;
        write_remaining_clusters();
        this->writer_.write(cluster<mm_hit>::end_token()); //write empty cluster as end token
        this->writer_.flush();
        std::cout << "CLUSTERER ENDED -------------------" << std::endl;    
    }
    trigger_clusterer() :
    pixel_lists_(MAX_PIXEL_COUNT),
    unfinished_clusters_count_(0),
    processed_hit_count_(0)
    {   
    }
    virtual ~trigger_clusterer() = default;
};

template <typename hit_type, typename state_type>
class abstract_trigger
{
    protected:
    double last_triggered_ = 0;
    const double UNTRIGGER_THRESHOLD_ = 100000000;  //in ns, set to 100 miliseconds (2 windows)
    std::optional<state_type> old_state_;
    public:
    virtual bool trigger(const state_type & new_state)
    {
        if (!old_state_.has_value())
        {
            old_state_.emplace(new_state);  //storing a copy of the current state
            return true;
        }
        old_state_.emplace(new_state);
        bool triggered = unwrapped_trigger(new_state);
        if(triggered)
        {
            last_triggered_ = new_state.last_hit_toa();
        }
        return triggered;

    }
    virtual bool unwrapped_trigger(const state_type & new_state) = 0;
    virtual bool untrigger(const state_type & new_state)
    {
        return new_state.last_hit_toa() - last_triggered_ > UNTRIGGER_THRESHOLD_;
    }
};

template <typename hit_type>
class frequency_diff_trigger : public abstract_trigger<hit_type, window_frequency_state<hit_type>>
{
    public:
    using state_type = window_frequency_state<hit_type>;
    virtual bool unwrapped_trigger(const state_type & new_state) override
    {
        double old_freq = this->old_state_->hit_count() * 1000 / this->old_state_->window_width_actual();
        double new_freq = new_state.hit_count() * 1000 / new_state.window_width_actual();
        std::cout << "OLD FREQ " << old_freq << " MHit/s" << std::endl;
        std::cout << "NEW_FREQ " << new_freq << " MHit/s" << std::endl;
        return ((old_freq > 0 && (new_freq / old_freq) > 2)
         || (new_freq > 0 && (old_freq / new_freq) > 2));       
    }
};
//TODO implement energy distr trigger but before that try to compile passing trigger type as an argument