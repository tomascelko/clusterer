#include "../data_flow/dataflow_package.h"
#include <queue>
#include "../data_structs/cluster.h"
#include "../data_structs/produced_cluster.h"
#include "../utils.h"
#include "../devices/current_device.h"
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
struct bb_cluster
{
    cluster<hit_type> cl;
    bbox bb;
    bool valid = true;
    void invalidate()
    {
        valid = false;
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
    bb_cluster(cluster<hit_type> && cl) :
    cl(cl)
    {
        compute_bb();
    }
};
template <typename hit_type, typename pipe_descriptor>
class cluster_merging : public i_data_consumer<produced_cluster<hit_type>>,
                            public i_data_producer<cluster<hit_type>>
{
    const uint32_t DEQUEUE_CHECK_INTERVAL = 20; //time when to check for dequeing from sorting queue 
    const double MERGE_TIME = 300.; //max time difference between cluster to be merged
    //const double SORTING_DEQUEUE_TIME = 1000000000.; //max unorderednes caused by parallelization
    const double MAX_CL_UNORD_TIME = 10000; //maximum unorderedness caused by clustering itself (merging to bigger cluster)
    const std::vector<coord> NINE_NEIGHBORS = { {-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1},
                                                { 0,  0} };
    pipe_writer<cluster<hit_type>> writer_;
    pipe_reader<produced_cluster<hit_type>> reader_;
    std::priority_queue<cluster<hit_type>, std::vector<cluster<hit_type>>, typename cluster<hit_type>::first_toa_comparer> priority_queue_;
    std::deque<bb_cluster<hit_type>> unfinished_border_clusters_; //we could keep here unbordering clusters as well 
                                                                   //in order to preserve time orderedness
    pipe_descriptor split_descr_;
    using bitmap_type = std::vector<std::vector<bool>>;
    bitmap_type neighboring_matrix_;
    uint64_t processed_non_border_count = 0;
    uint64_t processed_border_count = 0;
    i_sync_node* sync_node_;
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

    void process_old_border_cluster(cluster<hit_type>& old_cl)
    {
        bb_cluster bb_cl{std::move(old_cl)};
        for(uint32_t i = 0; i < unfinished_border_clusters_.size(); i++)
        {
            if (unfinished_border_clusters_[i].valid && can_merge(unfinished_border_clusters_[i], bb_cl))
            {
                bb_cl.merge_with(std::move(unfinished_border_clusters_[i]));
                unfinished_border_clusters_[i].invalidate();
                //break;
            }
        }
        unfinished_border_clusters_.emplace_back(std::move(bb_cl));
        
        
    }
    double dequeue_threshold()
    {
        return sync_node_->min_producer_time() - (MERGE_TIME + MAX_CL_UNORD_TIME);
    }
    void remove_invalid()
    {
        while((!unfinished_border_clusters_.empty()) && !unfinished_border_clusters_.front().valid)
                    unfinished_border_clusters_.pop_front();
    }
    void process_old_clusters(bool finishing = false)
    {

        double recent_dequeued_ftoa = 0.;
        while ((!priority_queue_.empty()) && (priority_queue_.top().last_toa() < dequeue_threshold() || finishing))
        {
            recent_dequeued_ftoa = priority_queue_.top().first_toa();
            cluster<hit_type> old_cl = std::move(const_cast<cluster<hit_type>&>(priority_queue_.top()));
            priority_queue_.pop();
            if(split_descr_.is_on_border(old_cl))
            {
                process_old_border_cluster(old_cl);
            }
            else
            {                        
                writer_.write(std::move(old_cl));
                ++processed_non_border_count;
            }
            //TODO possibly check for this only once in a border check interval
            remove_invalid();
            while((!unfinished_border_clusters_.empty()) && unfinished_border_clusters_.front().cl.last_toa() < recent_dequeued_ftoa - MAX_CL_UNORD_TIME)
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
    }
    void write_remaining_clusters()
    {
        process_old_clusters(true); //process all clusters on heap
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
    cluster_merging(pipe_descriptor split_descr, i_sync_node* sync_node) :
    split_descr_(split_descr),
    sync_node_(sync_node),
    neighboring_matrix_(current_chip::chip_type::size_x(), std::vector<bool>(current_chip::chip_type::size_y(), false))
    {
        typename cluster<hit_type>::first_toa_comparer first_toa_comp;
        priority_queue_ = std::priority_queue<cluster<hit_type>, std::vector<cluster<hit_type>>, typename cluster<hit_type>::first_toa_comparer>(first_toa_comp);

    }
    void connect_input(pipe<produced_cluster<hit_type>>* input_pipe) override
    {
        reader_ = pipe_reader<produced_cluster<hit_type>>(input_pipe);
    }
    virtual void connect_output(pipe<cluster<hit_type>>* output_pipe) override
    {
        writer_ = pipe_writer<cluster<hit_type>>(output_pipe);
    }
    uint64_t sorting_queue_size()
    {
        return priority_queue_.size();
    }
    virtual void start() override
    {
        uint64_t processed = 0;
        produced_cluster<hit_type> new_cl;
        uint32_t finish_producers_count = 0;
        reader_.read(new_cl); 
        while(finish_producers_count != split_descr_.pipe_count())
        {
            double current_toa = new_cl.cl().first_toa();
            priority_queue_.emplace(std::move(new_cl.cl()));
            sync_node_->update_producer_time(current_toa, new_cl.producer_id());
            ++processed;
            if(processed % DEQUEUE_CHECK_INTERVAL == 0)
                process_old_clusters();
            reader_.read(new_cl);
            while(!new_cl.is_valid())
            {
                ++finish_producers_count;
                if(finish_producers_count == split_descr_.pipe_count())
                    break;
                reader_.read(new_cl);
            }
        }
        write_remaining_clusters();
        std::cout << processed_border_count << " " << processed_non_border_count << std::endl; 
        writer_.write(cluster<hit_type>::end_token());
        writer_.flush();
        std::cout << "CLUSTER MERGING ENDED ---------------------" << std::endl;
    }

};

template <typename hit_type, typename clustering_node, typename pipe_descriptor>
class parallel_clusterer : public i_data_consumer<hit_type>,
                           public i_data_producer<cluster<hit_type>>, 
                           public i_sync_node                //performs the splitting work 
{                                                                    //and wraps the whole clustering process

    pipe_descriptor split_descr_;
    
    pipe_reader<hit_type> reader_;
    pipe_writer<cluster<hit_type>> writer_;
    cluster_merging<hit_type, pipe_descriptor>* merging_node_;
    std::vector<clustering_node*> clustering_nodes_;
    std::vector<pipe<hit_type>*> split_pipes_;
    std::vector<pipe_writer<hit_type>> split_writers_;
    const uint32_t PIPE_ID_PREFIX = 0x100;
    pipe<produced_cluster<hit_type>>* merging_pipe_; //needs to be concurrent to merge multiple producers
    double current_sync_toa_ = 0;
    std::vector<double> first_toas_by_producers_; //used 
    public:
    virtual void update_producer_time(double toa, uint32_t producer_id) override //consumer informs that it received data from producer
    {
        first_toas_by_producers_[producer_id] = toa;
    }
    virtual double min_producer_time() override //find last CONSUMED cluster by CONSUMER for each producer
    {       
        double min_toa = LONG_MAX;                           //for CONSUMERS ONLY
        for (uint16_t i = 0; i < first_toas_by_producers_.size(); i++)
        {
            if(first_toas_by_producers_[i] < min_toa)
            {
                min_toa = first_toas_by_producers_[i];
            }
        }
        return min_toa;
    }
    virtual void sync_time() override //checks all clusters and finds last PROCESSED hit by CLUSTERER
    {                                 //used for inter clusterer synchronization = for PRODUCERS ONLY
        double oldest_toa = LONG_MAX;
        for(auto & cl_node : clustering_nodes_)
        {
            double toa = cl_node->current_toa();
            if(toa < oldest_toa)
            {
                oldest_toa = toa;
            }
        }
        current_sync_toa_ = oldest_toa;
    }
    virtual double check_time() override
    {
        return current_sync_toa_;
    }
    virtual uint64_t queue_size()
    {
        return merging_node_->sorting_queue_size();
    }
    parallel_clusterer(pipe_descriptor split_descr) :
    split_descr_(split_descr),
    first_toas_by_producers_(split_descr.pipe_count(), 0),
    merging_node_(new cluster_merging<hit_type, pipe_descriptor>(split_descr, this)),
    merging_pipe_(new pipe<produced_cluster<hit_type>, moodycamel::ConcurrentQueue>(PIPE_ID_PREFIX))
    {
     
        merging_node_->connect_input(merging_pipe_); //output of merging node is decided after connecting the whole block
        for (uint32_t i = 0; i < split_descr_.pipe_count(); i++)
        {
            pipe<hit_type>* split_pipe = new pipe<hit_type> (PIPE_ID_PREFIX + i + 1);
            clustering_node* cl_node = new clustering_node(this, i);
            cl_node->connect_input(split_pipe);
            cl_node->connect_output(merging_pipe_);
            clustering_nodes_.emplace_back(cl_node);
            
            split_pipes_.push_back(split_pipe);
            split_writers_.emplace_back(pipe_writer<hit_type>(split_pipe));
        }
    }
    virtual void connect_input(pipe<hit_type>* input_pipe) override
    {
        reader_ = pipe_reader<hit_type>(input_pipe);
    }
    virtual void connect_output(pipe<cluster<hit_type>>* output_pipe) override
    {
        writer_ = pipe_writer<cluster<hit_type>>(output_pipe);
        merging_node_->connect_output(output_pipe);
    }
    virtual std::vector<i_data_node*> internal_nodes() override
    {
        std::vector<i_data_node*>int_nodes;
        int_nodes.assign(clustering_nodes_.begin(), clustering_nodes_.end());
        int_nodes.push_back(merging_node_);
        return int_nodes;        
    }
    virtual std::vector<abstract_pipe*> internal_pipes() override
    {
        std::vector<abstract_pipe*>int_pipes;
        int_pipes.assign(split_pipes_.begin(), split_pipes_.end());
        int_pipes.push_back(merging_pipe_);
        return int_pipes;
    }
    virtual void start() override
    {
        hit_type hit;
        reader_.read(hit);
        while(hit.is_valid())
        {
            split_writers_[split_descr_.get_pipe_index(hit)].write(std::move(hit));
            reader_.read(hit);
        }
        for (uint32_t i = 0; i < split_descr_.pipe_count(); i++)
        {
            split_writers_[i].write(hit_type::end_token());
            split_writers_[i].flush();
        }
        std::cout << "SPLITTER ENDED ---------------------" << std::endl;
    }

};
