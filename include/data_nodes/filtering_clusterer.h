#include "clusterer.h"
#pragma once
//#include "../data_structs/mm_hit.h"
template <typename mm_hit>// care, about unfinished energz cluster scope 
class energy_filtering_clusterer : public i_data_consumer<mm_hit>, public i_data_producer<cluster<mm_hit>>
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
    const double CLUSTER_CLOSING_DT = 200;   //time after which the cluster is closed (> DIFF_DT, because of delays in the datastream)
    const double CLUSTER_DIFF_DT = 200;     //time that marks the max difference of cluster last_toa()
    const uint32_t MAX_PIXEL_COUNT = 2 << 15;
    const uint32_t BUFFER_CHECK_INTERVAL = 16;
    const uint32_t WRITE_INTERVAL = 2 << 6;
    const double BUFFER_FORGET_DT = 500;
    const double CRITICAL_ENERGY = 70; //keV
    const std::vector<coord> EIGHT_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1}};
    bool finished_ = false;
    std::vector<cluster_it_list> pixel_lists_; 
    cluster_list unfinished_clusters_;
    uint32_t unfinished_clusters_count_;
    //how many clusters can be open at a time ?
    protected:
    uint64_t processed_hit_count_;
    pipe_reader<mm_hit> reader_;
    pipe_writer<cluster<mm_hit>> writer_;
    using buffer_type = std::deque<mm_hit>;
    //const uint32_t expected_buffer_size = 2 << ;
    buffer_type hit_buffer_;
    //double toa_crit_interval_end_ = 0;
    uint32_t hi_e_cl_count = 0; 
    enum class clusterer_state
    {
        processing,
        ignoring
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
        if(bigger_cluster.has_high_energy && smaller_cluster.has_high_energy)
        {
            --hi_e_cl_count;
        }
        bigger_cluster.has_high_energy = bigger_cluster.has_high_energy || smaller_cluster.has_high_energy;
        unfinished_clusters_.erase(smaller_cluster.self);
        --unfinished_clusters_count_;
        
    }
    std::vector<cluster_it> find_neighboring_clusters(const coord& base_coord, double toa, cluster_it& biggest_cluster)
    {
        std::vector<cluster_it> uniq_neighbor_cluster_its;
        uint32_t max_cluster_size = 0; 
        for (auto neighbor_offset : EIGHT_NEIGHBORS)   //check all neighbor indexes
        {       
            if(!base_coord.is_valid_neighbor(neighbor_offset))
                continue;
            uint32_t neighbor_index = neighbor_offset.linearize() + base_coord.linearize();
            //if(neighbor_index >= 0 && neighbor_index < MAX_PIXEL_COUNT)  //we check neighbor is not outside of the board
            //{           
                for (auto & neighbor_cl_it : pixel_lists_[neighbor_index])  //iterate over each cluster neighbor pixel can be in
                {
                    if(toa - CLUSTER_DIFF_DT < neighbor_cl_it->cl.last_toa() && !neighbor_cl_it->selected) //TODO order
                    {                                                                     //which of conditions is more likely to fail?
                        neighbor_cl_it->select();
                        uniq_neighbor_cluster_its.push_back(neighbor_cl_it);
                        if(neighbor_cl_it->cl.hits().size() > max_cluster_size)   //find biggest cluster for possible merging
                        {
                            max_cluster_size = neighbor_cl_it->cl.hits().size();
                            //std::cout << "before" << std::endl;
                            biggest_cluster = neighbor_cl_it;
                            //std::cout << "after" << std::endl;
                        }
                    }
                    //std::cout << "returned_if" << neighbor_index << std::endl;
                }
          //  }
        }
        //std::cout << "almost_returned" << std::endl;
        for (auto& neighbor_cluster_it : uniq_neighbor_cluster_its)
        {
            neighbor_cluster_it->unselect();    //unselect to prepare clusters for next neighbor search
        }
        //std::cout << "returning " << std::endl;
        return uniq_neighbor_cluster_its;    
    }
    void add_new_hit(mm_hit & hit, cluster_it & cluster_iterator)
    {
        //update cluster itself, assumes the cluster exists
        if(hit.e() > CRITICAL_ENERGY)
        {
            if(!cluster_iterator->has_high_energy)
            {
                ++hi_e_cl_count;
                cluster_iterator->has_high_energy = true;
            }
        }
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
            if (unfinished_clusters_.back().has_high_energy)
            {
                writer_.write(std::move(unfinished_clusters_.back().cl));
                --hi_e_cl_count;
            }
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
    void process_hit(mm_hit & hit) //used for benchmarking
    {
        hit_buffer_.push_back(hit);
            if(processed_hit_count_ % WRITE_INTERVAL == 0) 
                write_old_clusters(hit.toa());
            if(hi_e_cl_count == 0)                          //we written out all high energy clusters for now, 
                current_state = clusterer_state::ignoring;  //so we can start ignoring again
            if(processed_hit_count_ % BUFFER_CHECK_INTERVAL == 0)
                forget_old_hits(hit.toa());
            
            if(is_good_hit(hit) && current_state == clusterer_state::ignoring) //we just found interesting hit
            {                                                      //and we were in forgetting phase
                process_all_buffered();                            //so we look into the buffer backward in time
            }                                                      //and process all hits retrospectively
            if(current_state == clusterer_state::processing)
                clusterize_hit(hit);
    }
    void forget_old_hits(double current_toa) //removes old hits from buffer
    {
        while(hit_buffer_.size() > 0 && hit_buffer_.front().toa() <  current_toa - BUFFER_FORGET_DT)
        {
            hit_buffer_.pop_back();
        }
    }
    bool is_good_hit(const mm_hit & hit)
    {
        return hit.e() > CRITICAL_ENERGY;
    }
    void process_all_buffered()
    {
        current_state = clusterer_state::processing;
        for(uint32_t i = 0; i < hit_buffer_.size(); ++i)
        {
            clusterize_hit(hit_buffer_[i]);
            hit_buffer_.pop_front(); //we can remove the hit from buffer, 
        }                            //as it is now located inside of a unfinished clusters data structure

        //TODO when to start ignoring again ? set toa_crit_interval_end_
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
        reader_.read(hit);
        int num_hits = 0;
        while(hit.is_valid())
        {
            process_hit(hit);
            num_hits++;
            reader_.read(hit);
        }
        write_remaining_clusters();
        writer_.write(cluster<mm_hit>::end_token()); //write empty cluster as end token
        writer_.flush();
        std::cout << "CLUSTERER ENDED -------------------" << std::endl;    
    }
    virtual void connect_input(pipe<mm_hit>* in_pipe) override
    {
        reader_ = pipe_reader<mm_hit>(in_pipe);
    }
    virtual void connect_output(pipe<cluster<mm_hit>>* out_pipe) override
    {
        writer_ = pipe_writer<cluster<mm_hit>>(out_pipe);
    }
    energy_filtering_clusterer() :
    pixel_lists_(MAX_PIXEL_COUNT),
    unfinished_clusters_count_(0),
    processed_hit_count_(0)
    
    {   
    }
};