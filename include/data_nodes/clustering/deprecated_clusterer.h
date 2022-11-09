
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_set>
#include <set>
#include <map>
#include <queue>
#include "../../data_flow/dataflow_package.h"
#include "../../utils.h"
#include "../../data_structs/cluster.h"
#include "../../data_structs/mm_hit.h"
#include <deque>
/*template <typename hit_type, typename cluster>

class quad_tree_clusterer : public i_data_consumer<hit_type>, public i_data_producer<cluster>
{   
    using buffer_type = std::deque<hit_type>;
    private:
    std::unique_ptr<buffer_type> hit_deque_;
    pipe_reader<hit_type> reader_;
    pipe_writer<hit_type> writer_;
    std::vector<cluster> open_clusters_;

    bool can_be_added(const hit_type& hit, const cluster & cluster)
    {

    }
    void join_clusters(cluster & last_cluster, cluster & new_cluster)
    {
        last_cluster
    }
    public:
    default_clusterer(uint64_t buffer_size) :
    hit_buffer_(std::make_unique<buffer_type>(buffer_size))
    {
        hit_buffer_->reserve(buffer_size);
    }
    virtual void connect_input(pipe<hit_type>* input_pipe) override
    {
        reader_ = pipe_reader<hit_type>(input_pipe);
    }
    virtual void connect_output(pipe<hit_type>* output_pipe) override
    {
        writer_ = pipe_writer<hit_type>(output_pipe);
    }
    virtual void start() override
    {
        hit_type hit;
        while(!reader_.read(hit));
        while(hit.is_valid())
        { 
            bool added = false;
            cluster& last_cluster;
            for (uint32_t i = 0; i < open_clusters_.size(); i++)
            {
                if(can_be_added(hit, *it))
                {
                    if (added)
                    {
                        join_clusters(last_cluster, open_clusters_[i]);

                    }
                    else
                    {
                        open_clusters_[i]
                        added = true;
                        last_cluster = open_clusters_[i];
                    }
                }
            }
        }
        writer_.write(std::move(hit));
    }

};*/

class pixel_list_clusterer : public i_data_consumer<mm_hit>, public i_data_producer<cluster<mm_hit>>
{   

    using buffer_type = std::deque<mm_hit>;
    using cluster_id = uint64_t;
    struct cluster_time_id
    {
        static constexpr double epsilon = 0.01;
        double last_toa_;
        cluster_id id_;
        public:
        cluster_time_id(double toa, cluster_id id) :
        last_toa_(toa),
        id_(id)
        {}
        void update_last_toa(double new_toa)
        {
            last_toa_ = new_toa;
        }
        double last_toa() const
        {
            return last_toa_;
        }
        cluster_id id() const
        {
            return id_;
        }
        bool operator == (const cluster_time_id & other )
        {
            return std::abs(last_toa_ - other.last_toa()) < epsilon && id_ == other.id();
        }

    };
    template <typename mm_hit>
    class cluster_with_bit
    {
        cluster<mm_hit> cluster_;
        bool was_selected_ = false;
        public:
        cluster_with_bit()
        {}
        cluster_with_bit(cluster<mm_hit> && cluster):
            cluster_(std::move(cluster))
        {
        }
        void select()
        {
            was_selected_ = true;
        }
        void restore()
        {
            was_selected_ = false;
        }
        bool is_selected()
        {
            return was_selected_;
        }

        cluster<mm_hit> & cl()
        {
            return cluster_;
        }
    };
    struct last_toa_id_comparer
    {
        auto operator() (const cluster_time_id& left, const pixel_list_clusterer::cluster_time_id& right) const 
        {
            if(left.last_toa() < right.last_toa())
                return false;
            if(right.last_toa() < left.last_toa())
                return true;
            if(left.id() < right.id())
                return false;
            if(right.id() < left.id())
                return true;
            return false;  
        }
    };

    //TODO possible update = only use cluster id instead of timestamp, and get last toa from map (avoids duplication 
    //= saves memory but requires more time -> O(1) lookup with come constant)
    private:
    last_toa_id_comparer time_id_comparer_;
    const double CLUSTER_CLOSING_DT = 300;   //time after which the cluster is closed (> DIFF_DT, because of delays in the datastream)
    const double CLUSTER_DIFF_DT = 300;     //time that marks the max difference of cluster last_toa()
    const uint32_t MAX_PIXEL_COUNT = 2 << 15;
    const std::vector<coord> EIGHT_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1}};
    //std::unique_ptr<buffer_type> hit_deque_;
    pipe_reader<mm_hit> reader_;
    pipe_writer<cluster<mm_hit>> writer_;
    cluster_id new_cluster_id;

    std::vector<std::unordered_set<cluster_id>> pixel_lists_; //TODO possible ordered set? check find/(insert+delete) ratio
    //find and insert and delete are log(n) in priority queue (heap)
    //lets actually move items out of priority queue on delete, custom priority queue employed
    std::set<cluster_time_id, last_toa_id_comparer> toa_ordered_time_ids_; //delete and insert cluster after each pixelhit
    //how many clusters can be open at a time ?
    std::unordered_map<cluster_id, cluster_with_bit<mm_hit>> cluster_ids_map_;
    bool is_old(double last_toa, cluster_id id)
    {
        return cluster_ids_map_[id].cl().last_toa() < last_toa - CLUSTER_CLOSING_DT; 
    }
    void merge_clusters(cluster_id bigger_cluster_id, cluster_id smaller_cluster_id)
    {
        cluster<mm_hit> & bigger_cluster = cluster_ids_map_.at(bigger_cluster_id).cl();
        cluster<mm_hit> & smaller_cluster = cluster_ids_map_.at(smaller_cluster_id).cl();

        for (auto & hit : smaller_cluster.hits())
        {
            auto coord_linear = hit.coordinates().linearize(); 
            pixel_lists_[coord_linear].erase(smaller_cluster_id);
            pixel_lists_[coord_linear].insert(bigger_cluster_id);
        }

        bigger_cluster.hits().insert(bigger_cluster.hits().end(), std::make_move_iterator(smaller_cluster.hits().begin()),
        std::make_move_iterator(smaller_cluster.hits().end()));
        bigger_cluster.set_first_toa(std::min(bigger_cluster.first_toa(), smaller_cluster.first_toa()));
        bigger_cluster.set_last_toa(std::max(bigger_cluster.last_toa(), smaller_cluster.last_toa()));
        auto it = toa_ordered_time_ids_.find(cluster_time_id{smaller_cluster.last_toa(), smaller_cluster_id});
        toa_ordered_time_ids_.erase(it);

        cluster_ids_map_.erase(smaller_cluster_id);
        
        
    }
    //TODO update = instead of unordered set for uniqueness (requires lookup in neighbor_ID table - smaller), 
    //store bit in the cluster (requires lookup in ID table) but saves allocation of unordered set per processed pixel
    std::vector<cluster_id> find_neighboring_cluster_ids(const coord& coord, double toa)
    {

        std::vector<cluster_id> neighbor_cluster_ids;
        for (auto & neighbor_offset : EIGHT_NEIGHBORS)
        {
            int32_t neighbor_index = coord.linearize() + neighbor_offset.linearize();
            if(neighbor_index >= 0 && neighbor_index < MAX_PIXEL_COUNT) 
            {           
                for (auto cluster_index : pixel_lists_[neighbor_index])
                {
                    //we check that cluster is not
                    cluster_with_bit<mm_hit> & neighbor_cluster_bit =  cluster_ids_map_[cluster_index]; 
                    if(toa - CLUSTER_DIFF_DT < neighbor_cluster_bit.cl().last_toa())
                        if(!neighbor_cluster_bit.is_selected())
                        {
                            neighbor_cluster_bit.select();
                            neighbor_cluster_ids.push_back(cluster_index);
                        }
                }
            }
        }
        for (auto cluster_index : neighbor_cluster_ids)
        {
             cluster_ids_map_[cluster_index].restore();    
        }
        //std::vector<cluster_id> pixel_vector; 
        //pixel_vector.reserve(neighbor_cluster_ids.size());
        //pixel_vector.insert(vector.end(), neighbor_cluster_ids.begin(), neighbor_cluster_ids.end());
        return neighbor_cluster_ids;
        
    }
    cluster_id get_new_cluster_id()
    {
        return ++new_cluster_id;
    }
    void update_time_id(mm_hit & hit, cluster_id id)
    {
        double last_toa = cluster_ids_map_[id].cl().last_toa();
        if(hit.toa() > last_toa)
        {
            //we need to update position of time_id in the priority queue
            toa_ordered_time_ids_.erase(cluster_time_id{last_toa, id});
            toa_ordered_time_ids_.insert(cluster_time_id{hit.toa(), id});
        }
    }
    void add_new_hit(mm_hit & hit, cluster_id id)
    {
        //update cluster itself, assumes the cluster exists
        pixel_lists_[hit.coordinates().linearize()].insert(id);
        cluster_ids_map_.at(id).cl().add_hit(std::move(hit));

        //update the pixel list
    }
    uint32_t clusters_processed_ = 0;
    void write_old_clusters(const mm_hit& last_hit)
    {
        auto top = toa_ordered_time_ids_.begin();
        while (toa_ordered_time_ids_.size() > 0 && is_old(last_hit.toa(), top->id()))
        {
            for(const auto & hit : cluster_ids_map_.at(top->id()).cl().hits())
            {
                pixel_lists_[hit.coordinates().linearize()].erase(top->id());
            }

            writer_.write(std::move(cluster_ids_map_.at(top->id()).cl())); //TODO check if unordered map can move
            
            cluster_ids_map_.erase(top->id());

            toa_ordered_time_ids_.erase(top);
            top = toa_ordered_time_ids_.begin();
        }
    } 
    void process_hit(mm_hit & hit)
    {
        auto neighboring_cluster_ids = find_neighboring_cluster_ids(hit.coordinates(), hit.toa()); //copy elision should take place
        
        cluster_id clstr_id;
        switch(neighboring_cluster_ids.size())
        {
            case 0:
                clstr_id = get_new_cluster_id();
                //insert new cluster to priority queue
                toa_ordered_time_ids_.emplace(std::move(cluster_time_id{hit.toa(), clstr_id}));
                
                //create new cluster itself
                cluster_ids_map_.emplace(clstr_id, cluster_with_bit<mm_hit>{});
                
                add_new_hit(hit, clstr_id);
                break;
            case 1:
                //add pixel to cluster
                clstr_id = *neighboring_cluster_ids.begin();
                update_time_id(hit, clstr_id);
                add_new_hit(hit, clstr_id);         
                break;
            default:
                // we find the largest cluster and move hits from smaller clusters to the biggest one
                clstr_id = *(std::max_element(neighboring_cluster_ids.begin(), neighboring_cluster_ids.end(),[this](cluster_id left, cluster_id right)
                    {
                        return cluster_ids_map_[left].cl().hits().size() < cluster_ids_map_[right].cl().hits().size();
                    }));
                for (auto & neighbor_id : neighboring_cluster_ids)
                {   
                // TODO  merge clusters and then add pixel to it
                    if(neighbor_id != clstr_id)
                    {
                        merge_clusters(clstr_id, neighbor_id);
                    }
                }
                update_time_id(hit, clstr_id);
                add_new_hit(hit, clstr_id);
                break;


        }
    }
    public:
    pixel_list_clusterer(uint64_t buffer_size) :
    pixel_lists_(MAX_PIXEL_COUNT),
    new_cluster_id(0),
    toa_ordered_time_ids_(std::set<cluster_time_id, last_toa_id_comparer> (time_id_comparer_))
    //hit_buffer_(std::make_unique<buffer_type>(buffer_size))
    {
        //hit_buffer_->reserve(buffer_size);

        toa_ordered_time_ids_ = std::set<cluster_time_id, last_toa_id_comparer> (time_id_comparer_);
    }
    virtual void connect_input(pipe<mm_hit>* input_pipe) override
    {
        reader_ = pipe_reader<mm_hit>(input_pipe);
    }
    virtual void connect_output(pipe<cluster<mm_hit>>* output_pipe) override
    {
        writer_ = pipe_writer<cluster<mm_hit>>(output_pipe);
    }
    virtual void start() override
    {
        mm_hit hit;
        
        while(!reader_.read(hit));
        while(hit.is_valid())
        { 
            write_old_clusters(hit);
            process_hit(hit);
            while(!reader_.read(hit));
        }
        writer_.write(cluster<mm_hit>{}); //write empty cluster as end token
        writer_.flush();
        std::cout << "CLUSTER ENDED -------------------" << std::endl;
    }

};