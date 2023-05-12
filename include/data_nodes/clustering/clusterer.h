
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_set>
#include <set>
#include <map>
#include <queue>
#include "../../data_flow/dataflow_package.h"
#include "../../utils.h"
#include "../../data_structs/mm_hit.h"
#include "../../data_structs/node_args.h"
#include "../../utils.h"
#include "../../benchmark/i_time_measurable.h"
#include "../../benchmark/measuring_clock.h"
#include <deque>
#include <list>
#include "i_public_state_clusterer.h"
#pragma once
template <typename mm_hit>
struct unfinished_cluster;
using cluster_list = std::list<unfinished_cluster<mm_hit>>;
using cluster_it = typename cluster_list::iterator;
using cluster_it_list = typename std::list<cluster_it>;

template <typename mm_hit>
struct unfinished_cluster
{
    cluster<mm_hit> cl;
    std::vector<typename cluster_it_list::iterator> pixel_iterators;
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
template <template<class> typename cluster>
class pixel_list_clusterer : public i_simple_consumer<mm_hit>, 
                             public i_multi_producer<cluster<mm_hit>>, 
                             public i_time_measurable,
                             public i_public_state_clusterer<unfinished_cluster<mm_hit>>
{   

    protected:    
    
    const double CLUSTER_CLOSING_DT = 300;   //time after which the cluster is closed (> DIFF_DT, because of delays in the datastream)
    double cluster_diff_dt = 200;     //time that marks the max difference of cluster last_toa()
    const std::vector<coord> EIGHT_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1},
                                                { 0, -1}, { 0, 0}, { 0, 1},
                                                { 1, -1}, { 1, 0}, { 1, 1}};

    std::vector<cluster_it_list> pixel_lists_; 
    cluster_list unfinished_clusters_;
    uint32_t unfinished_clusters_count_;
    bool finished_ = false;
    protected:
    const uint32_t WRITE_INTERVAL = 2 << 3;
    uint64_t processed_hit_count_;
    measuring_clock * clock_;
    double current_toa_ = 0;
    uint32_t tile_size_;
    
    bool is_old(double last_toa, const cluster<mm_hit> & cl)
    {
        return cl.last_toa() < last_toa - CLUSTER_CLOSING_DT; 
    }
    void merge_clusters(unfinished_cluster<mm_hit> & bigger_cluster, unfinished_cluster<mm_hit> & smaller_cluster) 
    //merging clusters to biggest cluster will however disrupt time orderedness - after merging bigger cluster can lower its first toa
    //which causes time unorderedness, can hovewer improve performance
    //TODO try always merging to left (smaller toa)
    {
        
        for(auto & pix_it : smaller_cluster.pixel_iterators) //update iterator
        {
            *pix_it = bigger_cluster.self;
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
    std::vector<cluster_it> find_neighboring_clusters(const coord& base_coord, double toa, cluster_it& biggest_cluster)
    {
        std::vector<cluster_it> uniq_neighbor_cluster_its;
        //uint32_t max_cluster_size = 0; 
        double min_toa = ULONG_MAX;
        for (auto neighbor_offset : EIGHT_NEIGHBORS)   //check all neighbor indexes
        {       
            if(!base_coord.is_valid_neighbor(neighbor_offset, tile_size_))
                continue;
            //error in computing neighbor offset linearize() - does not account for tile size TODOOOOOOOOOo
            uint32_t neighbor_index = neighbor_offset.linearize_tiled(tile_size_) + base_coord.linearize(tile_size_); 
            /*std::cout << "--------" << std::endl;
            std::cout << base_coord.x_ << " " << base_coord.y_ << std::endl;
            std::cout << neighbor_offset.x_ << " " << neighbor_offset.y_ << std::endl; 
            std::cout << neighbor_index << std::endl;
            std::cout << neighbor_offset.linearize(tile_size_) << std::endl;  
            std::cout << base_coord.linearize(tile_size_) << std::endl;*/       
                for (auto & neighbor_cl_it : pixel_lists_[neighbor_index])  //iterate over each cluster neighbor pixel can be in
                //TODO do reverse iteration and break when finding a match - as there cannot two "already mergable" clusters on a single neighbor pixel
                {
                    if(std::abs(toa - neighbor_cl_it->cl.last_toa()) < cluster_diff_dt && !neighbor_cl_it->selected) //TODO order
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
    public:
    //i_public_state_clusterer
    std::string name() override 
    {
        return "clusterer";
    }
    virtual bool open_cl_exists() override 
    {
        return unfinished_clusters_.begin() != unfinished_clusters_.end();
    }
    virtual std::vector<cluster_it> get_all_unfinished_clusters() override
    {
        std::vector<cluster_it> unfinished_its;
        for(auto it = unfinished_clusters_.begin(); it != unfinished_clusters_.end(); ++it)
        {
            unfinished_its.push_back(it);
        }
        return unfinished_its;
    }
    //standard API
    void add_new_hit(mm_hit & hit, cluster_it & cluster_iterator)
    {
        //update cluster itself, assumes the cluster exists
        auto &  target_pixel_list = pixel_lists_[hit.coordinates().linearize(tile_size_)];
        target_pixel_list.push_front(cluster_iterator);
        cluster_iterator->pixel_iterators.push_back(target_pixel_list.begin()); //beware, we need to push in the same order,
                                                                                //as we push hits in addhit and in merging
        cluster_iterator->cl.add_hit(std::move(hit));
        //update the pixel list
    }

    
    virtual void write_old_clusters(double hit_toa = 0)
    {
        //old clusters should be at the end of the list
        while (unfinished_clusters_count_ > 0  && (is_old(hit_toa, unfinished_clusters_.back().cl) || finished_))
        {
            unfinished_cluster<mm_hit> & current = unfinished_clusters_.back();
            auto & current_hits = current.cl.hits(); 
            for(uint32_t i = 0; i < current.pixel_iterators.size(); i++) //update iterator
            {
                auto & pixel_list_row = pixel_lists_[current_hits[i].coordinates().linearize(tile_size_)];
                pixel_list_row.erase(current.pixel_iterators[i]); //FIXME pixel_list_rows should be appended in other direction
            }
            current_toa_ = unfinished_clusters_.back().cl.first_toa();
            this->writer_.write(std::move(unfinished_clusters_.back().cl));
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
                //merge clusters and then add pixel to it
                    if(neighboring_cluster_it != target_cluster)
                    {
                        merge_clusters(*target_cluster, *neighboring_cluster_it);
                    }
                }
                break;
        }
        add_new_hit(hit, target_cluster);
        ++processed_hit_count_;
    }
    void write_remaining_clusters()
    {
        finished_ = true;
        write_old_clusters();
        this->writer_.close();
    }
    pixel_list_clusterer(node_descriptor<mm_hit, cluster<mm_hit>>* node_descr, const node_args & args) :
    pixel_lists_(current_chip::chip_type::size_x() * current_chip::chip_type::size_y() / (
        args.get_arg<int>(name(), "tile_size") * args.get_arg<int>(name(), "tile_size"))),
    tile_size_(args.get_arg<int>(name(), "tile_size")),
    unfinished_clusters_count_(0),
    processed_hit_count_(0),
    cluster_diff_dt(args.get_arg<double>(name(), "max_dt")),
    i_multi_producer<cluster<mm_hit>>(node_descr->split_descr)
    {  
        return;
    }
    pixel_list_clusterer(const node_args & args) :
    pixel_lists_(current_chip::chip_type::size_x() * current_chip::chip_type::size_y() / (
         args.get_arg<int>(name(), "tile_size") * args.get_arg<int>(name(), "tile_size"))),
    tile_size_(args.get_arg<int>(name(), "tile_size")),
    unfinished_clusters_count_(0),
    processed_hit_count_(0),
    cluster_diff_dt(args.get_arg<double>(name(), "max_dt")),
    i_multi_producer<cluster<mm_hit>>(new trivial_split_descriptor<cluster<mm_hit>>())
    {  
        return;
    }
    double current_toa()
    {
        return current_toa_;
    }
    void reset()
    {
        pixel_lists_.clear();
        unfinished_clusters_.clear();
        unfinished_clusters_count_ = 0;
        finished_ = false;
    }
    void set_tile(uint32_t tile_size)
    {

        tile_size_ = tile_size;
        reset();
        pixel_lists_ = std::vector<cluster_it_list>(current_chip::chip_type::size_x() * 
            current_chip::chip_type::size_y() / (tile_size * tile_size));
    }
    virtual void start() override
    {
        mm_hit hit;
        while(!reader_.read(hit));
        //clock_->start();
        while(hit.is_valid())
        {
            if(processed_hit_count_ % WRITE_INTERVAL == 0) 
                write_old_clusters(hit.toa());
            process_hit(hit);
            while(!reader_.read(hit));
        }   
        write_remaining_clusters();
        //clock_->stop_and_report("clusterer");

        //std::cout << "CLUSTERER ENDED ---------- " << processed_hit_count_ <<" hits processed" <<std::endl;
    }
    virtual ~pixel_list_clusterer() = default;
    //i_time_measurable
    virtual void prepare_clock(measuring_clock* clock) override
    {
        clock_ = clock;
    }
};