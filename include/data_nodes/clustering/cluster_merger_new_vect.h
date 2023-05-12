#include "../../data_flow/dataflow_package.h"
#include <queue>
#include "../../data_structs/cluster.h"
#include "../../utils.h"
#include "../../devices/current_device.h"
#include "../../benchmark/i_time_measurable.h"
#include "cluster_merger_new.h"
#pragma once

template <typename hit_type, typename cluster_reference_type = cluster<hit_type>>
struct bb_cluster
{
    cluster_reference_type cl;
    bbox bb;
    bool valid = true;
    void invalidate()
    {
        valid = false;
    }
    void set_bb(bbox&& bb)
    {
        this.bb = bb;
    }
    void compute_bb()
    {
        for(auto & hit : cl.hits())
        {
            if(hit.x() > bb.right_bot.x())
                bb.right_bot.x_ = hit.x();
            if(hit.x() < bb.left_top.x())
                bb.left_top.x_ = hit.x();
            if(hit.y() > bb.right_bot.y())
                bb.right_bot.y_ = hit.y();
            if(hit.y() < bb.left_top.y())
                bb.left_top.y_ = hit.y();
        }
        bb.left_top = coord(std::max(bb.left_top.x() - 1, (int)coord::MIN_VALUE),
                    std::max(bb.left_top.y() - 1, (int)coord::MIN_VALUE));
        //bb.left_top. = std::max(bb.left_top.y() - 1, (int)coord::MIN_VALUE);
        bb.right_bot = coord(std::min(bb.right_bot.x() + 1, coord::MAX_VALUE - 1),
                        std::min(bb.right_bot.y() + 1, coord::MAX_VALUE - 1));
        
    }
    void merge_with(bb_cluster && other)
    {
        cl.merge_with(std::move(other.cl));
        bb.left_top = coord(std::min(bb.left_top.x(), other.bb.left_top.x()),
                           std::min(bb.left_top.y(), other.bb.left_top.y()));
        bb.right_bot = coord(std::max(bb.right_bot.x(), other.bb.right_bot.x()),
                           std::max(bb.right_bot.y(), other.bb.right_bot.y()));
                           other.invalidate();
        
    }
    bb_cluster(cluster_reference_type && cl) :  //CHECK ME
    cl(cl)
    {
        compute_bb();
    }
};

template <typename hit_type>
class cluster_merging : public i_data_consumer<cluster<hit_type>>,
                            public i_data_producer<cluster<hit_type>>,
                            public i_time_measurable  
{
    const uint32_t DEQUEUE_CHECK_INTERVAL = 10; //time when to check for dequeing from sorting queue 
    const double MERGE_TIME = 300.; //max time difference between cluster to be merged
    //const double SORTING_DEQUEUE_TIME = 1000000000.; //max unorderednes caused by parallelization
    const double MAX_CLUSTER_TIMESPAN = 2000.; //maximum total timespan of a cluster, must be set to speedup merging
    const std::vector<coord> NINE_NEIGHBORS = { {-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1},
                                                { 0,  0} };
    pipe_writer<cluster<hit_type>> writer_;
    multi_cluster_pipe_reader<hit_type> reader_;
    std::priority_queue<cluster<hit_type>, std::vector<cluster<hit_type>>, typename cluster<hit_type>::first_toa_comparer> priority_queue_;
    std::deque<bb_cluster<hit_type>> unfinished_border_clusters_; //we could keep here unbordering clusters as well 
                                                                   //in order to preserve time orderedness
    pipe_descriptor<hit_type>* split_descr_;
    using bitmap_type = std::vector<std::vector<bool>>;
    bitmap_type neighboring_matrix_;
    uint64_t processed_non_border_count = 0;
    uint64_t processed_border_count = 0;

    measuring_clock* clock_;
    bool are_spatially_neighboring(const bb_cluster<hit_type> & bb_cl_1, const bb_cluster<hit_type> & bb_cl_2)
    {
        const bb_cluster<hit_type> & bigger_bb_cl = bb_cl_1.cl.hits().size() < bb_cl_2.cl.hits().size() ? bb_cl_2 : bb_cl_1;
        const bb_cluster<hit_type> & smaller_bb_cl = bb_cl_1.cl.hits().size() < bb_cl_2.cl.hits().size() ? bb_cl_1 : bb_cl_2;
        
        bbox intersection_bb = bb_cl_1.bb.intersect_with(bb_cl_2.bb);
        for(auto & hit : bigger_bb_cl.cl.hits())
        {
            if (intersection_bb.lies_in_bb(hit.coordinates()))
                neighboring_matrix_[hit.x()][hit.y()] = true;
        }
        bool is_neighbor = false;
        for(auto & hit : smaller_bb_cl.cl.hits())
        {
            if (intersection_bb.lies_in_bb(hit.coordinates()))
            {
                for(auto & neighbor_offset : NINE_NEIGHBORS)
                {
                    if(hit.coordinates().is_valid_neighbor(neighbor_offset) &&
                    neighboring_matrix_[hit.x() + neighbor_offset.x()][hit.y() + neighbor_offset.y()] == true)
                    {
                        is_neighbor = true;
                        break;
                    }
                }
            }
            if (is_neighbor)
                break;
        }
        for(auto & hit : bigger_bb_cl.cl.hits())
        {
            //TODO possile to skip when optimizing
            /*if (intersection_bb.lies_in_bb(hit.coordinates()))
            {
            if(neighboring_matrix_[hit.x()][hit.y()] == false)
                break;
            }*/
            neighboring_matrix_[hit.x()][hit.y()] = false;

        }
        return is_neighbor;
    }
    bool can_merge(const bb_cluster<hit_type>& bb_cl_1, const bb_cluster<hit_type> & bb_cl_2)
    {

        if(std::abs(bb_cl_1.cl.first_toa() - bb_cl_2.cl.last_toa()) > MERGE_TIME && 
           std::abs(bb_cl_1.cl.last_toa() - bb_cl_2.cl.first_toa()) > MERGE_TIME)
        {
            return false;  //clusters are too far apart timewise
        }
        if(!bb_cl_1.bb.overlaps_with(bb_cl_2.bb))
        {
            return false; //clusters do not have overlapping bounding boxes
        }
        return are_spatially_neighboring(bb_cl_1, bb_cl_2);
        //possibly make 4 linear lists vert, horiz, diag2, diag2 and then 8 neighbors should be actual neighbors
        //in some of those 4 lists but beware of ends of line 
        //TODO implement proper "can merge" check
        return true;
    }

    void process_border_cluster(cluster<hit_type>& old_cl)
    {
        bb_cluster<hit_type> bb_cl{std::move(old_cl)};
        for(int32_t i = unfinished_border_clusters_.size() - 1; i >= 0; --i)
        {
            if(unfinished_border_clusters_[i].valid && 
                unfinished_border_clusters_[i].cl.first_toa() < bb_cl.cl.first_toa() - (MAX_CLUSTER_TIMESPAN + MERGE_TIME))
                break;
            if (unfinished_border_clusters_[i].valid && can_merge(unfinished_border_clusters_[i], bb_cl))
            {
                bb_cl.merge_with(std::move(unfinished_border_clusters_[i]));
                unfinished_border_clusters_[i].invalidate();
            }
            

        }
        unfinished_border_clusters_.emplace_back(std::move(bb_cl));
        
        
    }
    void remove_invalid()
    {
        while((!unfinished_border_clusters_.empty()) && !unfinished_border_clusters_.front().valid)
                    unfinished_border_clusters_.pop_front();
    }
    void process_cluster(cluster<hit_type> && new_cl)
    {

        double recent_dequeued_ftoa = new_cl.first_toa();
            if(split_descr_->is_on_border(new_cl))
            {
                process_border_cluster(new_cl);
            }
            else
            {                        
                writer_.write(std::move(new_cl));
                ++processed_non_border_count;
            }
            //TODO possibly check for this only once in a border check interval
            remove_invalid();
            while((!unfinished_border_clusters_.empty()) && unfinished_border_clusters_.front().cl.last_toa() < recent_dequeued_ftoa - MERGE_TIME)
            {
                if(unfinished_border_clusters_.front().valid)
                {
                    writer_.write(std::move(unfinished_border_clusters_.front().cl));
                    ++processed_border_count;
                }
                unfinished_border_clusters_.pop_front();
                remove_invalid();
            }
    }
    void write_remaining_clusters()
    {
        while((!unfinished_border_clusters_.empty())) //process last clusters on border
        {
            remove_invalid();
            if(!unfinished_border_clusters_.empty()) 
            {
                writer_.write(std::move(unfinished_border_clusters_.front().cl));
                ++processed_border_count;
                unfinished_border_clusters_.pop_front();
            }
        }
    }
public:
    cluster_merging(pipe_descriptor<hit_type>* split_descr) :
    split_descr_(split_descr),
    neighboring_matrix_(current_chip::chip_type::size_x(), std::vector<bool>(current_chip::chip_type::size_y(), false))
    {
        typename cluster<hit_type>::first_toa_comparer first_toa_comp;
        priority_queue_ = std::priority_queue<cluster<hit_type>, std::vector<cluster<hit_type>>, typename cluster<hit_type>::first_toa_comparer>(first_toa_comp);

    }
    std::string name()
    {
        return "merger";
    }
    void connect_input(default_pipe<cluster<hit_type>>* input_pipe) override
    {
        reader_.add_pipe(input_pipe);
    }
    void connect_output(default_pipe<cluster<hit_type>>* output_pipe) override
    {
        writer_ = pipe_writer<cluster<hit_type>>(output_pipe);
    }
    uint64_t sorting_queue_size()
    {
        return priority_queue_.size();
    }
    void prepare_clock( measuring_clock * clock)
    {
        clock_ = clock;
    }
    virtual void start() override
    {
        uint64_t processed = 0;
        cluster<hit_type> new_cl;
        uint32_t finish_producers_count = 0;
        reader_.read(new_cl); 
        clock_->start(); //FIXME interferes with paralell clusterer clock.start()
        while(new_cl.is_valid())
        {
            //double current_toa = new_cl.cl().first_toa();
            //sync_node_->update_producer_time(current_toa, new_cl.producer_id());
            ++processed;
            //if(processed % DEQUEUE_CHECK_INTERVAL == 0)
                process_cluster(std::move(new_cl));
            reader_.read(new_cl);
        }
        write_remaining_clusters();
        std::cout << processed_border_count << " " << processed_non_border_count << std::endl; 
        clock_->stop_and_report("parallel_clusterer");
        writer_.write(cluster<hit_type>::end_token());
        writer_.flush();
        //std::cout << "CLUSTER MERGING ENDED ---------------------" << std::endl;
    }
    virtual ~cluster_merging() = default;

};