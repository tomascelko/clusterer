#include "../../data_flow/dataflow_package.h"
#include <queue>
#include "../../data_structs/cluster.h"
#include "../../utils.h"
#include "../../devices/current_device.h"
#include "../../benchmark/i_time_measurable.h"
#pragma once
//auxiliary bounding box around the cluster, to quickly separate non-mergeable clusters
struct bbox
{
    coord left_top;
    coord right_bot;
    bbox() : left_top(coord::MAX_VALUE, coord::MAX_VALUE),
             right_bot(coord::MIN_VALUE, coord::MIN_VALUE)
    {
    }
    bbox(coord &&left_top, coord &&right_bot) : left_top(left_top),
                                                right_bot(right_bot)
    {
    }
    template <typename hit_type>
    bbox(const cluster<hit_type> &cluster) : left_top(coord::MAX_VALUE, coord::MAX_VALUE),
                                             right_bot(coord::MIN_VALUE, coord::MIN_VALUE)
    {

        for (auto &hit : cluster.hits())
        {
            if (hit.x() > right_bot.x())
                right_bot.x_ = hit.x();
            if (hit.x() < left_top.x())
                left_top.x_ = hit.x();
            if (hit.y() > right_bot.y())
                right_bot.y_ = hit.y();
            if (hit.y() < left_top.y())
                left_top.y_ = hit.y();
        }
        //IMPORTANT - the bounding boxes are extended by 1 PIXEL to each side
        //therefore the mergability condition can be represented as a intersection check
        left_top = coord(std::max(left_top.x() - 1, (int)coord::MIN_VALUE),
                         std::max(left_top.y() - 1, (int)coord::MIN_VALUE));
        //also extend the box to the other side
        right_bot = coord(std::min(right_bot.x() + 1, coord::MAX_VALUE - 1),
                          std::min(right_bot.y() + 1, coord::MAX_VALUE - 1));
    }
    //a simple check if two bounding boxes overlap
    bool overlaps_with(const bbox &other) const
    {
        if (right_bot.x() < other.left_top.x() || other.right_bot.x() < left_top.x())
            return false;
        if (right_bot.y() < other.left_top.y() || other.right_bot.y() < left_top.y())
            return false;
        return true;
    }
    //computes intersection of two bounding boxes to narrow down the area
    //where the intersection can occur
    bbox intersect_with(const bbox &other) const
    {
        coord new_left_top(std::max(left_top.x(), other.left_top.x()),
                           std::max(left_top.y(), other.left_top.y()));
        coord new_right_bot(std::min(right_bot.x(), other.right_bot.x()),
                            std::min(right_bot.y(), other.right_bot.y()));
        return bbox(std::move(new_left_top), std::move(new_right_bot));
    }
    bool lies_in_bb(coord coordinate) const
    {
        return coordinate.x() >= left_top.x() &&
               coordinate.y() >= left_top.y() &&
               coordinate.x() <= right_bot.x() &&
               coordinate.y() <= right_bot.y();
    }
};
//an auxiliary wrapper around the cluster which contains additional cluster metadata
template <typename hit_type, typename cluster_reference_type = cluster<hit_type>>
struct bb_cluster
{
    //cluster
    cluster_reference_type cl;
    //its bounding box
    bbox bb;
    //validity flag
    bool valid = true;
    //bordering flag
    bool bordering_;
    //offset to precious bordering cluster
    uint32_t bordering_offset_;
    bool bordering()
    {
        return bordering_;
    }
    uint32_t bordering_offset()
    {
        return bordering_offset_;
    }
    void invalidate()
    {
        valid = false;
    }
    void set_bb(bbox &&bb)
    {
        this.bb = bb;
    }
    void compute_bb()
    {
        for (auto &hit : cl.hits())
        {
            if (hit.x() > bb.right_bot.x())
                bb.right_bot.x_ = hit.x();
            if (hit.x() < bb.left_top.x())
                bb.left_top.x_ = hit.x();
            if (hit.y() > bb.right_bot.y())
                bb.right_bot.y_ = hit.y();
            if (hit.y() < bb.left_top.y())
                bb.left_top.y_ = hit.y();
        }
        bb.left_top = coord(std::max(bb.left_top.x() - 1, (int)coord::MIN_VALUE),
                            std::max(bb.left_top.y() - 1, (int)coord::MIN_VALUE));
        bb.right_bot = coord(std::min(bb.right_bot.x() + 1, coord::MAX_VALUE - 1),
                             std::min(bb.right_bot.y() + 1, coord::MAX_VALUE - 1));
    }
    void merge_with(bb_cluster &&other)
    {
        cl.merge_with(std::move(other.cl));
        bb.left_top = coord(std::min(bb.left_top.x(), other.bb.left_top.x()),
                            std::min(bb.left_top.y(), other.bb.left_top.y()));
        bb.right_bot = coord(std::max(bb.right_bot.x(), other.bb.right_bot.x()),
                             std::max(bb.right_bot.y(), other.bb.right_bot.y()));
        other.invalidate();
    }
    bb_cluster(cluster_reference_type &&cl, uint32_t bordering_offset = 0, bool should_compute = true) : // CHECK ME
                                                                                                         cl(cl),
                                                                                                         bordering_offset_(bordering_offset),
                                                                                                         bordering_(should_compute)
    {
        if (should_compute)
            compute_bb();
    }
};

//a node which implements the merging
template <typename hit_type>
class cluster_merging : public i_data_consumer<cluster<hit_type>>,
                        public i_multi_producer<cluster<hit_type>>,
                        public i_time_measurable
{
    const uint32_t DEQUEUE_CHECK_INTERVAL = 10; // time when to check for dequeing from sorting queue
    double merge_time = 200.;             // max time difference between cluster to be merged
    const double MAX_CLUSTER_TIMESPAN = 2000.; // maximum total timespan of a cluster, must be set to speedup merging
    const std::vector<coord> NINE_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 0}, {0, 1}, {1, -1}, {1, 0}, {1, 1}, {0, 0}};
    // pipe_writer<cluster<hit_type>> writer_;
    multi_pipe_reader<cluster<hit_type>> reader_;
    std::priority_queue<cluster<hit_type>, std::vector<cluster<hit_type>>, typename cluster<hit_type>::first_toa_comparer> priority_queue_;
    std::deque<bb_cluster<hit_type>> unfinished_border_clusters_; // we could keep here unbordering clusters as well
                                                                  // in order to preserve time orderedness
    node_descriptor<cluster<hit_type>, cluster<hit_type>> *node_descr_;
    using bitmap_type = std::vector<std::vector<double>>;
    bitmap_type neighboring_matrix_;
    uint64_t processed_non_border_count = 0;
    uint64_t processed_border_count = 0;
    uint32_t bordering_offset = 1;
    measuring_clock *clock_;
    bool are_spatially_neighboring(const bb_cluster<hit_type> &bb_cl_1, const bb_cluster<hit_type> &bb_cl_2)
    {
        const bb_cluster<hit_type> &bigger_bb_cl = bb_cl_1.cl.hits().size() < bb_cl_2.cl.hits().size() ? bb_cl_2 : bb_cl_1;
        const bb_cluster<hit_type> &smaller_bb_cl = bb_cl_1.cl.hits().size() < bb_cl_2.cl.hits().size() ? bb_cl_1 : bb_cl_2;

        bbox intersection_bb = bb_cl_1.bb.intersect_with(bb_cl_2.bb);
        for (auto &hit : bigger_bb_cl.cl.hits())
        {
            if (intersection_bb.lies_in_bb(hit.coordinates()))
                neighboring_matrix_[hit.x()][hit.y()] = hit.toa();
        }
        bool is_neighbor = false;
        for (auto &hit : smaller_bb_cl.cl.hits())
        {
            if (intersection_bb.lies_in_bb(hit.coordinates()))
            {
                for (auto &neighbor_offset : NINE_NEIGHBORS)
                {
                    if (hit.coordinates().is_valid_neighbor(neighbor_offset) &&
                        (neighboring_matrix_[hit.x() + neighbor_offset.x()][hit.y() + neighbor_offset.y()] >= 0) &&
                        std::abs(neighboring_matrix_[hit.x() + neighbor_offset.x()][hit.y() + neighbor_offset.y()] - hit.toa()) < merge_time)
                    {
                        is_neighbor = true;
                        break;
                    }
                }
            }
            if (is_neighbor)
                break;
        }
        for (auto &hit : bigger_bb_cl.cl.hits())
        {
            neighboring_matrix_[hit.x()][hit.y()] = -1;
        }
        return is_neighbor;
    }
    bool can_merge(const bb_cluster<hit_type> &bb_cl_1, const bb_cluster<hit_type> &bb_cl_2)
    {

        if (std::abs(bb_cl_1.cl.first_toa() - bb_cl_2.cl.last_toa()) > merge_time &&
            std::abs(bb_cl_1.cl.last_toa() - bb_cl_2.cl.first_toa()) > merge_time)
        {
            return false; // clusters are too far apart timewise
        }
        if (!bb_cl_1.bb.overlaps_with(bb_cl_2.bb))
        {
            return false; // clusters do not have overlapping bounding boxes
        }
        return are_spatially_neighboring(bb_cl_1, bb_cl_2); //full proper neighboring check
        
    }

    void process_border_cluster(cluster<hit_type> &old_cl)
    {
        bb_cluster<hit_type> bb_cl{std::move(old_cl), bordering_offset};
        std::vector<uint32_t> merge_indices;
        for (int32_t i = unfinished_border_clusters_.size() - bordering_offset; i >= 0; i -= unfinished_border_clusters_[i].bordering_offset())
        {
            if (unfinished_border_clusters_[i].valid &&
                unfinished_border_clusters_[i].cl.first_toa() < bb_cl.cl.first_toa() - (MAX_CLUSTER_TIMESPAN + merge_time))
                break;
            if (unfinished_border_clusters_[i].valid && can_merge(unfinished_border_clusters_[i], bb_cl))
            {
                merge_indices.push_back(i); 
            }
        }
        if (merge_indices.size() == 0)
            unfinished_border_clusters_.emplace_back(std::move(bb_cl));
        else
        {
            uint32_t oldest_to_merge = merge_indices.back(); // retrieve oldest cluster for merging
            merge_indices.pop_back();
            unfinished_border_clusters_[oldest_to_merge].merge_with(std::move(bb_cl)); // merge current with oldest
            for (auto merge_ind : merge_indices)                                       // possibly also merge other suitable clustrers with the oldest
            {
                unfinished_border_clusters_[oldest_to_merge].merge_with(std::move(unfinished_border_clusters_[merge_ind]));
                unfinished_border_clusters_[merge_ind].invalidate();
            }
        }
    }
    void remove_invalid()
    {
        while ((!unfinished_border_clusters_.empty()) && !unfinished_border_clusters_.front().valid)
            unfinished_border_clusters_.pop_front();
    }
    std::vector<cluster<hit_type>> cls_;
    void process_cluster(cluster<hit_type> &&new_cl)
    {
        double recent_dequeued_ftoa = new_cl.first_toa();
        if (node_descr_->merge_descr->is_on_border(new_cl) && !node_descr_->merge_descr->should_be_forwarded(new_cl))
        {
            process_border_cluster(new_cl);
            bordering_offset = 1;
        }
        else
        {
            if (unfinished_border_clusters_.size() > 0)
            {
                bb_cluster<hit_type> bb_cl{std::move(new_cl), bordering_offset, false};
                unfinished_border_clusters_.emplace_back(std::move(bb_cl));
            }
            else
            {
                this->writer_.write(std::move(new_cl));
            }
            ++bordering_offset;
            ++processed_non_border_count;
        }
        write_old_clusters(recent_dequeued_ftoa);
    }
    void write_old_clusters(double last_toa)
    {
        remove_invalid();
        while ((!unfinished_border_clusters_.empty()) && (!unfinished_border_clusters_.front().bordering() || unfinished_border_clusters_.front().cl.last_toa() < last_toa - merge_time))
        {
            if (unfinished_border_clusters_.front().valid)
            {
                this->writer_.write(std::move(unfinished_border_clusters_.front().cl));
                ++processed_border_count;
            }
            unfinished_border_clusters_.pop_front();
            remove_invalid();
        }
    }
    void write_remaining_clusters()
    {
        while ((!unfinished_border_clusters_.empty())) // process last clusters on border
        {
            remove_invalid();
            if (!unfinished_border_clusters_.empty())
            {
                this->writer_.write(std::move(unfinished_border_clusters_.front().cl));
                ++processed_border_count;
                unfinished_border_clusters_.pop_front();
            }
        }
    }
    const double EMPTY_TIME_VALUE_ = -1.;

public:
    cluster_merging(node_descriptor<cluster<hit_type>, cluster<hit_type>> *node_descr, const node_args & args) : node_descr_(node_descr),
                                                                                         neighboring_matrix_(current_chip::chip_type::size_x(), std::vector<double>(current_chip::chip_type::size_y(), EMPTY_TIME_VALUE_)),
                                                                                         merge_time(args.get_arg<double>("clusterer", "max_dt")),
                                                                                         i_multi_producer<cluster<hit_type>>(node_descr->split_descr)
    {
        typename cluster<hit_type>::first_toa_comparer first_toa_comp;
        priority_queue_ = std::priority_queue<cluster<hit_type>, std::vector<cluster<hit_type>>, typename cluster<hit_type>::first_toa_comparer>(first_toa_comp);
    }

    std::string name()
    {
        return "merger";
    }
    void connect_input(default_pipe<cluster<hit_type>> *input_pipe) override
    {
        reader_.add_pipe(input_pipe);
    }
    uint64_t sorting_queue_size()
    {
        return priority_queue_.size();
    }
    void prepare_clock(measuring_clock *clock)
    {
        clock_ = clock;
    }
    virtual void start() override
    {
        uint64_t processed = 0;
        cluster<hit_type> new_cl;
        uint32_t finish_producers_count = 0;
        reader_.read(new_cl);
        while (new_cl.is_valid())
        {
            ++processed;
            process_cluster(std::move(new_cl));
            reader_.read(new_cl);
        }
        write_remaining_clusters();
        
        this->writer_.close();
        
    }
    virtual ~cluster_merging() = default;
};