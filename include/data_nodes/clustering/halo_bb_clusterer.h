
#include "clusterer.h"
#include "../../data_structs/cluster.h"
#include "cluster_merger.h"

//note : bb_cluster_type should always be some reference type (eg. std::reference_wrapper<T>)
//as we need to store reference to an existing clusters inside the actual clusterer
template<typename hit_type, typename clusterer_type, typename bb_cluster_type = bb_cluster<hit_type, std::reference_wrapper<cluster<hit_type>>>>
class halo_buffer_clusterer : public i_simple_consumer<hit_type>, public i_data_producer<cluster<hit_type>>, public i_time_measurable
{ 
    protected:
    pixel_list_clusterer<cluster> clustering_node_;
    measuring_clock * clock_;
    double time_window_size_;
    double window_start_time_;
    std::unique_ptr<clusterer_type> clusterer_;
    std::vector<hit_type> hit_buffer_;
    std::vector<bool> hit_buffer_valid_;
    const std::vector<double> THRESHOLDS_ = {120}; //ordered in descending order
    const double MIN_CLUSTERING_DT = 200.;
    const uint32_t WRITE_INTERVAL = 10;
    uint64_t processed_hit_count_;
    void clusterize_remaining() 
    {
        uint32_t processed = 0;
       for(uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            if(hit_buffer_valid_[i])
            {
                clusterer_->process_hit(hit_buffer_[i]);
                hit_buffer_valid_[i] = false;
                if(processed % WRITE_INTERVAL == 0)
                    clusterer_->write_old_clusters(hit_buffer_[i].toa());
                ++processed;
            }
        }
    }
    void clusterize_by_threshold(double threshold)
    {
        //clusterer_->set_tile(2);
        for(uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            if(hit_buffer_[i].e() >= threshold && hit_buffer_valid_[i])
            {
                clusterer_->process_hit(hit_buffer_[i]);
                hit_buffer_valid_[i] = false;
            }
        }
        std::vector<bbox> bounding_boxes;
        auto open_clusters_its = clusterer_->get_all_unfinished_clusters();
        for(auto it = open_clusters_its.begin(); it != open_clusters_its.end(); ++it)
        {
            bounding_boxes.push_back(bbox((*it)->cl)); //TODO bbox will take const ref cluster in constructor
            //and compute the bounding box in it
            //thenm use open clusters its when add new hit is called
            //bb_clusters.back().compute_bb();
        }
        int32_t bb_start_index = open_clusters_its.size() - 1;
        for(uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            if(bb_start_index < 0)
                break;
            if(hit_buffer_valid_[i])
            {
                int32_t bb_current_index = bb_start_index;
                while (bb_current_index >= 0 
                && open_clusters_its[bb_current_index]->cl.first_toa() - hit_buffer_[i].toa() < MIN_CLUSTERING_DT
                && hit_buffer_[i].toa() - open_clusters_its[bb_current_index]->cl.last_toa() < MIN_CLUSTERING_DT)
                {
                    if(bounding_boxes[bb_current_index].lies_in_bb(hit_buffer_[i].coordinates()))
                    {
                        clusterer_->add_new_hit(hit_buffer_[i], open_clusters_its[bb_current_index]);
                        hit_buffer_valid_[i] = false;
                        break;
                    }
                    --bb_current_index;
                }   
                if(bb_current_index >= 0 && hit_buffer_[i].toa() - open_clusters_its[bb_current_index]->cl.last_toa() > MIN_CLUSTERING_DT)
                {
                    --bb_start_index;
                } 
            }
        }

    }
    void process_buffer()
    {
        for (const auto threshold : THRESHOLDS_)
        {
            clusterize_by_threshold(threshold);
        }
        clusterize_remaining();
        write_old_clusters(std::numeric_limits<double>::max());
        hit_buffer_.clear();
        hit_buffer_valid_.clear();
    }
    public:
    void write_old_clusters(double recent_toa)
    {
        clusterer_->write_old_clusters(recent_toa);
    }
    void write_remaining_clusters()
    {
        //TODO only call when there are some open clusters (clustering has been called)
        clusterer_->write_remaining_clusters();
    }
    void process_hit(hit_type & hit) //can be called from outside
    {

        if(hit.toa() - window_start_time_ > time_window_size_)
        {
            process_buffer();
            window_start_time_ = hit.toa();
        }
        hit_buffer_.emplace_back(hit);
        hit_buffer_valid_.emplace_back(true);
    }
    std::string name() override
    {
        return "halo_bb_clusterer";
    }
    virtual void start() override
    {
        hit_type hit;
        this->reader_.read(hit);
        clock_->start();
        clusterer_->set_tile(2);
        while(hit.is_valid())
        {
            process_hit(hit);
            this->reader_.read(hit);
            ++processed_hit_count_;
        }   
        write_remaining_clusters();

        clock_->stop_and_report("clusterer");

        std::cout << "HALO BB CLUSTERER ENDED ---------- " <<std::endl;
    }
    template <typename... underlying_clusterer_args_type>
    halo_buffer_clusterer(double time_window_size = 1000000, underlying_clusterer_args_type... underlying_clusterer_args) :
    clusterer_(std::make_unique<clusterer_type>(underlying_clusterer_args...)),
    time_window_size_(time_window_size),
    window_start_time_(0)
    
    {
       //clusterer_->connect_output(this->writer_.pipe()); //clusterer_ is performing the output of this node
    }
    virtual ~halo_buffer_clusterer() = default;
    virtual void prepare_clock(measuring_clock* clock) override
    {
        clock_ = clock;
    }
    void connect_output(default_pipe<cluster<hit_type>> * pipe) override
    {
         clusterer_->connect_output(pipe);  //underlying clusterer is writing to the output of this node
    }
};