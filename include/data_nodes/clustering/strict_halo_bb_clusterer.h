
#include "clusterer.h"
#include "../../data_structs/cluster.h"
#include "cluster_merger.h"
template <typename hit_type, typename clusterer_type, typename bb_cluster_type = bb_cluster<hit_type>>
class strict_halo_buffer_clusterer : public i_simple_consumer<hit_type>, public i_simple_producer<cluster<hit_type>>, public i_time_measurable
{
protected:
    pixel_list_clusterer<cluster> clustering_node_;
    measuring_clock *clock_;
    double time_window_size_;
    double window_start_time_;
    std::unique_ptr<clusterer_type> clusterer_;
    std::vector<hit_type> hit_buffer_;
    std::vector<bool> hit_buffer_valid_;
    const std::vector<double> THRESHOLDS_ = {60}; // ordered in descending order
    const double MIN_CLUSTERING_DT = 200.;
    void write_all_sorted()
    {
    }
    void clusterize_remaining()
    {
        for (uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            if (hit_buffer_[i].e() >= threshold && hit_buffer_valid_[i])
            {
                clusterer_->process_hit(std::move(hit_buffer[i]));
                hit_buffer_valid_[i] = false;
            }
        }
        std::vector<cluster<hit_type>> &clusters = clusterer_->get_all_open_clusters();
        for (auto &cluster : clusters)
        {
            sorter_.enqueue(std::move(cluster));
        }
    }
    void clusterize_by_threshold(double threshold)
    {
        clusterer->set_tile(4);
        for (uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            if (hit_buffer_[i].e() >= threshold && hit_buffer_valid_[i])
            {
                clusterer_->process_hit(std::move(hit_buffer[i]));
                hit_buffer_valid_[i] = false;
            }
        }
        std::vector<bb_cluster_type> &bb_clusters;
        while (clusterer_->open_cl_exists())
        {
            bb_clusters.push_back(bb_cluster_type{clusterer_->get_oldest_cluster()});
            bb_clusters.back().compute_bb();
        }
        for (uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            if (hit_buffer_valid_[i])
            {
                uint32_t bb_index = 0;
                while (bb_clusters[bb_index].cl.first_toa() - hit_buffer_[i].toa() < MIN_CLUSTERING_DT)
                {
                    if (bb_clusters[bb_index].bb.lies_in_bb(hit_buffer_[i]))
                    {
                        bb_clusters[bb_index].cl.add_hit(hit_buffer_[i]);
                        hit_buffer_valid_[i] = false;
                        break;
                    }
                }
            }
        }
        for (auto &bb_cluster : bb_clusters)
        {
            sorter_.enqueue(std::move(bb_cluster));
        }
    }
    void process_buffer()
    {
        for (const auto threshold : THRESHOLDS_)
        {
            clusterize_by_threshold(threshold)
        }
        clusterize_remaining();
        write_all_sorted();
        hit_buffer_.clear();
        hit_buffer_valid_.clear();
    }

public:
    void write_old_clusters()
    {
        // writes all clusters processed located in heap data structure
    }
    void process_hit(hit_type &&hit) // can be called from outside
    {

        if (hit.toa() - window_start_time_ > time_window_size_)
            process_buffer();
        hit_buffer_.emplace_back(hit);
        hit_buffer_valid_.emplace_back(true);
    }
    virtual void start() override
    {
        hit_type hit;
        reader_.read(hit);
        clock_->start();
        while (hit.is_valid())
        {
            if (processed_hit_count_ % WRITE_INTERVAL == 0)
                write_old_clusters(hit.toa());
            process_hit(hit);
            reader_.read(hit);
        }
        write_remaining_clusters();
        clock_->stop_and_report("clusterer");
        this->writer_.write(cluster<mm_hit>::end_token()); // write empty cluster as end token
        this->writer_.flush();

    }
    halo_buffer_clusterer(double time_window_size, clusterer_type *clusterer) : clusterer_(std::unique_ptr<clusterer_type>(clusterer)),
                                                                                time_window_size_(time_window_size),
                                                                                window_start_time_(0)
    {
    }
    virtual ~halo_buffer_clusterer() = default;
    virtual void prepare_clock(measuring_clock *clock) override
    {
        clock_ = clock;
    }
};