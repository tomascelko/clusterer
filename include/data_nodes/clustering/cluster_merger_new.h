#include "../../data_flow/dataflow_package.h"
#include <queue>
#include "../../data_structs/cluster.h"
#include "../../utils.h"
#include "../../devices/current_device.h"
#include "../../benchmark/i_time_measurable.h"
#include <list>
#pragma once
struct bbox
{
    coord left_top;
    coord right_bot;
    bbox() :
    left_top(coord::MAX_VALUE, coord::MAX_VALUE),
    right_bot(coord::MIN_VALUE, coord::MIN_VALUE)
    {}
    bbox(coord && left_top, coord && right_bot) :
    left_top(left_top),
    right_bot(right_bot)
    {}
    template <typename hit_type>
    bbox(const cluster<hit_type> & cluster)
    {
        for(auto & hit : cluster.hits())
        {
            if(hit.x() > right_bot.x())
                right_bot.x_ = hit.x();
            if(hit.x() < left_top.x())
                left_top.x_ = hit.x();
            if(hit.y() > right_bot.y())
                right_bot.y_ = hit.y();
            if(hit.y() < left_top.y())
                left_top.y_ = hit.y();
        }
        left_top = coord(std::max(left_top.x() - 1, (int)coord::MIN_VALUE),
                    std::max(left_top.y() - 1, (int)coord::MIN_VALUE));
        //bb.left_top. = std::max(bb.left_top.y() - 1, (int)coord::MIN_VALUE);
        right_bot = coord(std::min(right_bot.x() + 1, coord::MAX_VALUE - 1),
                        std::min(right_bot.y() + 1, coord::MAX_VALUE - 1));
    }

    bool overlaps_with(const bbox& other) const
    {
        if (right_bot.x() < other.left_top.x() || other.right_bot.x() < left_top.x())
            return false;
        if (right_bot.y() < other.left_top.y() || other.right_bot.y() < left_top.y())
            return false;
        return true;
    }
    bbox intersect_with(const bbox & other) const
    {
        coord new_left_top(std::max(left_top.x(),other.left_top.x()), 
                           std::max(left_top.y(),other.left_top.y()));
        coord new_right_bot(std::min(right_bot.x(),other.right_bot.x()), 
                           std::min(right_bot.y(),other.right_bot.y()));
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
template <typename hit_type>
struct cluster_border_wrapper
{
    cluster<hit_type> cl;
    bool is_on_border;
    cluster_border_wrapper(cluster<hit_type> && cluster, bool is_on_border) :
    cl(cluster),
    is_on_border(is_on_border)
    {
    }

};

template <typename hit_type>
struct bb_cluster_it
{
    using cl_it_type = typename std::list<cluster_border_wrapper<hit_type>>::iterator;
    using bb_cl_it_type = typename std::list<bb_cluster_it<hit_type>>::iterator;
    cl_it_type cl_it;
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
        for(auto & hit : cl_it->cl.hits())
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
    void merge_with(bb_cl_it_type other)
    {
        bb.left_top = coord(std::min(bb.left_top.x(), other->bb.left_top.x()),
                           std::min(bb.left_top.y(), other->bb.left_top.y()));
        bb.right_bot = coord(std::max(bb.right_bot.x(), other->bb.right_bot.x()),
                           std::max(bb.right_bot.y(), other->bb.right_bot.y()));
        cl_it->cl.merge_with(std::move(other->cl_it->cl));
    
        
        
    }
    void merge_with(bb_cluster_it<hit_type> && other)
    {
        bb.left_top = coord(std::min(bb.left_top.x(), other.bb.left_top.x()),
                           std::min(bb.left_top.y(), other.bb.left_top.y()));
        bb.right_bot = coord(std::max(bb.right_bot.x(), other.bb.right_bot.x()),
                           std::max(bb.right_bot.y(), other.bb.right_bot.y()));
        cl_it->cl.merge_with(std::move(other.cl_it->cl));
        
        
    }
    bb_cluster_it(cl_it_type cl_it) :  //CHECK ME
    cl_it(cl_it)
    {
        compute_bb();
    }
};


template <typename hit_type>
class cluster_merging_new : public i_data_consumer<cluster<hit_type>>,
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
    using bb_cl_type = bb_cluster_it<hit_type>;
    using bb_cl_it_type = typename std::list<bb_cl_type>::iterator;
    using cl_it_type =  typename std::list<cluster_border_wrapper<hit_type>>::iterator;
    using bitmap_type = std::vector<std::vector<bool>>;
    std::list<cluster_border_wrapper<hit_type>> unfinished_clusters_; 
    std::list<bb_cl_type> unfinished_border_cl_its_;
    pipe_descriptor<hit_type>* split_descr_;
    bitmap_type neighboring_matrix_;
    uint64_t processed_non_border_count = 0;
    uint64_t processed_border_count = 0;

    measuring_clock* clock_;
    bool are_spatially_neighboring(const bb_cluster_it<hit_type> & bb_cl_1, const bb_cluster_it<hit_type> & bb_cl_2)
    {
        const bb_cluster_it<hit_type> & bigger_bb_cl = bb_cl_1.cl_it->cl.hits().size() < bb_cl_2.cl_it->cl.hits().size() ? bb_cl_2 : bb_cl_1;
        const bb_cluster_it<hit_type> & smaller_bb_cl = bb_cl_1.cl_it->cl.hits().size() < bb_cl_2.cl_it->cl.hits().size() ? bb_cl_1 : bb_cl_2;
        
        bbox intersection_bb = bb_cl_1.bb.intersect_with(bb_cl_2.bb);
        for(auto & hit : bigger_bb_cl.cl_it->cl.hits())
        {
            if (intersection_bb.lies_in_bb(hit.coordinates()))
                neighboring_matrix_[hit.x()][hit.y()] = true;
        }
        bool is_neighbor = false;
        for(auto & hit : smaller_bb_cl.cl_it->cl.hits())
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
        for(auto & hit : bigger_bb_cl.cl_it->cl.hits())
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
    bool can_merge(const bb_cluster_it<hit_type>& bb_cl_1, const bb_cluster_it<hit_type> & bb_cl_2)
    {

        if(std::abs(bb_cl_1.cl_it->cl.first_toa() - bb_cl_2.cl_it->cl.last_toa()) > MERGE_TIME && 
           std::abs(bb_cl_1.cl_it->cl.last_toa() - bb_cl_2.cl_it->cl.first_toa()) > MERGE_TIME)
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

    void process_border_cluster(cl_it_type cl_it)
    {
        bb_cluster_it<hit_type> bb_cl{cl_it};
        std::vector<bb_cl_it_type> its_to_merge;
        for(auto border_it = unfinished_border_cl_its_.begin(); border_it != unfinished_border_cl_its_.end(); ++border_it)
        {
            if(border_it->cl_it->cl.first_toa() < bb_cl.cl_it->cl.first_toa() - (MAX_CLUSTER_TIMESPAN + MERGE_TIME))
                break;
            if (can_merge(*border_it, bb_cl))
            {
                its_to_merge.push_back(border_it);
                
            }
        }
        if(its_to_merge.size() == 0)
            unfinished_border_cl_its_.emplace_front(std::move(bb_cl));
        else
        {
            its_to_merge[its_to_merge.size() - 1]->merge_with(std::move(bb_cl));
            unfinished_clusters_.erase(cl_it);
            for (uint32_t i = 0; i < its_to_merge.size() - 1; ++i)
            {
                its_to_merge[its_to_merge.size() - 1]->merge_with(its_to_merge[i]);
                unfinished_clusters_.erase(its_to_merge[i]->cl_it);
                unfinished_border_cl_its_.erase(its_to_merge[i]);
            }

        }
           
    }

    void process_cluster(cluster<hit_type> && new_cl)
    {

        double recent_dequeued_ftoa = new_cl.first_toa();
            if(split_descr_->is_on_border(new_cl))
            {
                unfinished_clusters_.emplace_front(cluster_border_wrapper<hit_type>(std::move(new_cl), true));
                process_border_cluster(unfinished_clusters_.begin());
            }
            else
            {
                unfinished_clusters_.emplace_front(cluster_border_wrapper<hit_type>(std::move(new_cl), false));
            }
            while((!unfinished_clusters_.empty()) && unfinished_clusters_.back().cl.last_toa() < recent_dequeued_ftoa - MERGE_TIME)
            {
                if(unfinished_clusters_.back().is_on_border)
                {
                    unfinished_border_cl_its_.pop_back(); //if cluster being removed is on border, it must be at the end of the border list
                }
                writer_.write(std::move(unfinished_clusters_.back().cl));
                processed_border_count++;
                unfinished_clusters_.pop_back();
            }
            
    }
    void write_remaining_clusters()
    {
        unfinished_border_cl_its_.clear();
        while((!unfinished_clusters_.empty()))
        {
            if(!unfinished_clusters_.empty()) 
            {
                if(unfinished_clusters_.back().is_on_border)
                {
                    processed_border_count++;
                    unfinished_border_cl_its_.pop_back(); //if cluster being removed is on border, it must be at the end of the border list
                }
                writer_.write(std::move(unfinished_clusters_.back().cl));
                
                unfinished_clusters_.pop_back();
            }
        }
    }
public:
    cluster_merging_new(pipe_descriptor<hit_type>* split_descr) :
    split_descr_(split_descr),
    neighboring_matrix_(current_chip::chip_type::size_x(), std::vector<bool>(current_chip::chip_type::size_y(), false))
    {
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
            ++processed;
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
    virtual ~cluster_merging_new() = default;

};