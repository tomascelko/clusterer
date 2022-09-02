
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_set>
#include <set>
#include <map>
#include <queue>
#include "../data_flow/dataflow_package.h"
#include "../utils.h"
#include "../data_structs/cluster.h"
#include "../data_structs/mm_hit.h"
#include "../utils.h"
#include <deque>
#include <list>
#pragma once
class pixel_list_clusterer : public i_data_consumer<mm_hit>, public i_data_producer<cluster<mm_hit>>
{   

    protected:    
    template <typename mm_hit>
    struct unfinished_cluster;
    using cluster_list = std::list<unfinished_cluster<mm_hit>>;
    using cluster_it = cluster_list::iterator;
    using cluster_it_list = std::list<cluster_it>;
    template <typename mm_hit>
    struct unfinished_cluster
    {
        cluster<mm_hit> cl;
        std::vector<cluster_it_list::iterator> pixel_iterators;
        cluster_it self;
        bool selected = false;
        unfinished_cluster()
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
    const std::vector<coord> EIGHT_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1}};

    std::vector<cluster_it_list> pixel_lists_; 
    cluster_list unfinished_clusters_;
    uint32_t unfinished_clusters_count_;
    bool finished_ = false;
    //how many clusters can be open at a time ?
    protected:
    const uint32_t WRITE_INTERVAL = 2 << 6;
    uint64_t processed_hit_count_;
    pipe_reader<mm_hit> reader_;
    pipe_writer<cluster<mm_hit>> writer_;
    bool is_old(double last_toa, const cluster<mm_hit> & cluster)
    {
        return cluster.last_toa() < last_toa - CLUSTER_CLOSING_DT; 
    }
    void merge_clusters(unfinished_cluster<mm_hit> & bigger_cluster, unfinished_cluster<mm_hit> & smaller_cluster) 
    //merging clusters to biggest cluster will however disrupt time orderedness - after merging bigger cluster can lower its first toa
    //which causes time unorderedness, can hovewer improve performance
    //TODO try always merging to left (smaller toa)
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

        unfinished_clusters_.erase(smaller_cluster.self);
        --unfinished_clusters_count_;
        
    }
    //TODO update = instead of unordered set for uniqueness (requires lookup in neighbor_ID table - smaller), 
    //store bit in the cluster (requires lookup in ID table) but saves allocation of unordered set per processed pixel
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
        auto &  target_pixel_list = pixel_lists_[hit.coordinates().linearize()];
        target_pixel_list.push_front(cluster_iterator);
        cluster_iterator->pixel_iterators.push_back(target_pixel_list.begin()); //beware, we need to push in the same order,
                                                                                //as we push hits in addhit and in merging
        cluster_iterator->cl.add_hit(std::move(hit));
        //update the pixel list
    }
    
    void write_old_clusters(double hit_toa = 0)
    {
        //old clusters should be at the end of the list
        while (unfinished_clusters_count_ > 0  && (is_old(hit_toa, unfinished_clusters_.back().cl) || finished_))
        {
            unfinished_cluster<mm_hit> & current = unfinished_clusters_.back();
            auto & current_hits = current.cl.hits(); 
            for(uint32_t i = 0; i < current.pixel_iterators.size(); i++) //update iterator
            {
                auto & pixel_list_row = pixel_lists_[current_hits[i].coordinates().linearize()];
                pixel_list_row.erase(current.pixel_iterators[i]); //FIXME pixel_list_rows should be appended in other direction
            }
            writer_.write(std::move(unfinished_clusters_.back().cl));
            unfinished_clusters_.pop_back();
            --unfinished_clusters_count_;
        }
    } 
    void process_hit(mm_hit & hit)
    {
        cluster_it target_cluster = unfinished_clusters_.end();
        const auto neighboring_clusters = find_neighboring_clusters(hit.coordinates(), hit.toa(), target_cluster);
        switch(neighboring_clusters.size())
        {
            case 0:
                
                unfinished_clusters_.emplace_front(unfinished_cluster<mm_hit>{});
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
    void write_remaining_clusters()
    {
        finished_ = true;
        write_old_clusters();
    }
    public:
    pixel_list_clusterer() :
    pixel_lists_(MAX_PIXEL_COUNT),
    unfinished_clusters_count_(0),
    processed_hit_count_(0)   
    {

        
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
            if(processed_hit_count_ % WRITE_INTERVAL == 0) 
                write_old_clusters(hit.toa());
            process_hit(hit);
            while(!reader_.read(hit));
        }   
        write_remaining_clusters();
        writer_.write(cluster<mm_hit>::end_token()); //write empty cluster as end token
        writer_.flush();
        std::cout << "CLUSTERER ENDED -------------------" << std::endl;
    }

};