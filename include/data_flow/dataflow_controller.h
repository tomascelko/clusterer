#include <vector>
#include <memory>
#include "i_data_node.h"
#include "i_data_consumer.h"
#include "i_data_producer.h"
#include "pipe.h"
#include "abstract_pipe.h"
#include "i_controlable_source.h"
#include <thread>
#include <iostream>
#include "pipe_descriptor.h"
#include "../data_structs/cluster.h"
#include "../data_structs/mm_hit.h"
#include "../data_structs/burda_hit.h"
#include "../data_nodes/window_processing/default_window_feature_vector.h"

#pragma once
class dataflow_controller
{
    using node_pointer = std::unique_ptr<i_data_node>;
    using pipe_pointer = std::unique_ptr<abstract_pipe>;
    using descriptor_map_type = std::map<std::string, std::unique_ptr<abstract_node_descriptor>>;
private:
    std::vector<node_pointer> nodes_;
    //TODO add map<string, node_pointer> ktora bude mapovat meno na uzol
    std::vector<pipe_pointer> pipes_;
    std::vector<std::thread> threads_;
    uint32_t memory_control_counter_ = 0;
    std::vector<i_controlable_source*> data_sources_;
    std::ostream& state_info_out_stream_ = std::cout;
    descriptor_map_type node_descriptor_map_;
    bool debug_mode_;
    void register_node(i_data_node* item)
    {
        i_controlable_source* controlable_source_opt = dynamic_cast<i_controlable_source*>(item);
        if(controlable_source_opt)
            data_sources_.push_back(controlable_source_opt);
        nodes_.emplace_back(std::move(std::unique_ptr<i_data_node>(item)));
    }
    //template <typename data_type>
    void register_pipe(abstract_pipe* pipe)
    {
        pipes_.emplace_back(std::move(std::unique_ptr<abstract_pipe>(pipe)));
    }
public:
    dataflow_controller(std::map<std::string, abstract_node_descriptor * > & descriptors, bool debug = false):
    debug_mode_(debug)
    {
        for (const auto & id_and_descriptor : descriptors)
        {
            node_descriptor_map_.emplace(std::make_pair(id_and_descriptor.first, 
            std::unique_ptr<abstract_node_descriptor>(id_and_descriptor.second))); 
        }
    };

    std::vector<i_data_node*> nodes()
    {
        std::vector<i_data_node*> nodes;
        nodes.reserve(nodes_.size());
        for (auto & node : nodes_)
        {
            nodes.push_back(node.get());
        }
        return nodes;
    }
    void remove_all()
    {
        nodes_.clear();
        pipes_.clear();
        data_sources_.clear();
        std::cout << "dataflow finished" << std::endl;
    }

    template <typename data_type>
    default_pipe<data_type>* connect_nodes(i_data_producer<data_type>* producer, i_data_consumer<data_type>* consumer)
    {
        //std::cout << "templated" << std::endl;
        default_pipe<data_type>* connecting_pipe = new default_pipe<data_type>(producer->name() + std::to_string(producer->get_id()) + "---->" + 
            consumer->name() + "_" + std::to_string(consumer->get_id()), producer, consumer);
            //std::cout << "pipe_created" << std::endl;
        return connect_nodes<data_type>(producer, consumer, connecting_pipe);


    }
    
    void connect_nodes(i_data_node* producer, i_data_node* consumer)
    {
        //TODO create workflow where we can find 
        //std::cout << "entering" << std::endl;

        if(dynamic_cast<i_data_producer<mm_hit>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<mm_hit>*>(producer), dynamic_cast<i_data_consumer<mm_hit>*>(consumer));
        else if(dynamic_cast<i_data_producer<burda_hit>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<burda_hit>*>(producer), dynamic_cast<i_data_consumer<burda_hit>*>(consumer));
        else if (dynamic_cast<i_data_producer<cluster<mm_hit>>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<cluster<mm_hit>>*>(producer), dynamic_cast<i_data_consumer<cluster<mm_hit>>*>(consumer));
        else if (dynamic_cast<i_data_producer<default_window_feature_vector<mm_hit>>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<default_window_feature_vector<mm_hit>>*>(producer), 
            dynamic_cast<i_data_consumer<default_window_feature_vector<mm_hit>>*>(consumer));
        else throw std::invalid_argument("provided data node produces unsupported data type");
    }
    void add_node(i_data_node * data_node)
    {
        //todo save map with name of node
        register_node(data_node);
        //adding internal sub-nodes of a node
        for(auto int_node : data_node->internal_nodes())
        {
            register_node(int_node);
        }
        //adding internal pipes inside of a node
        for(auto int_pipe : data_node->internal_pipes())
        {
            register_pipe(int_pipe);
        }
    }
    template <typename data_type>
    default_pipe<data_type>* connect_nodes(i_data_producer<data_type>* producer, i_data_consumer<data_type>* consumer, default_pipe<data_type>* pipe)
    {         
        register_pipe(pipe);
        producer->connect_output(pipe);
        consumer->connect_input(pipe);
        return pipe;
    }

    uint64_t get_used_memory()
    {
        
        uint64_t used_memory = 0;
        for (auto & pipe : pipes_)
        {
            used_memory += pipe->bytes_used();
        }
        return used_memory;
    }
    void print_pipe_state()
    {

        for (auto & pipe : pipes_)
        {
            state_info_out_stream_ << pipe->name() << ": used MB:" << pipe->bytes_used()/(2<<(20-1)) << std::endl;
        }
        state_info_out_stream_ << "----------------" << std::endl;

    }
    void control_memory_usage(uint64_t min_memory = 2ull << 31, double max_pipe_memory = 2ull << 28) //29
    {
        uint64_t used_memory = get_used_memory();
        //uint64_t free_memory = get_free_memory();
        const uint64_t EPSILON_MEMORY = 2ull << 27; //CHANGED FROM 27
        memory_control_counter_ ++;
        //std::cout<< "USED MEMORY" << used_memory / 1000000. << std::endl;
        //std::cout << "FREE MEMORY" << free_memory / 1000000. << std::endl;
        //if(memory_control_counter_ % 2 == 0)
        //   state_info_out_stream_ << used_memory / 1000000. << " MB" << std::endl;
        for (auto & node : nodes_)
        {
            auto controlable = dynamic_cast<i_controlable_source*>(node.get());
            if(controlable != nullptr)
            {
                if(/*free_memory < min_memory &&*/ used_memory > max_pipe_memory - EPSILON_MEMORY)
                {
                    controlable->pause_production();
                }
                if(used_memory < max_pipe_memory - EPSILON_MEMORY)
                {
                    controlable->continue_production();    
                }
            }
            
        }
    }
    bool is_full_memory(uint64_t max_pipe_memory = 2ull << 30)
    {
        uint64_t used_memory = get_used_memory();
        return used_memory > max_pipe_memory;
    }
    std::vector<uint64_t> get_memory_per_lane()
    {
        std::vector<uint64_t> occupied_memory;
        for (auto & pipe : pipes_)
        {
            uint32_t lane_id = 0;
            auto controlable = dynamic_cast<i_controlable_source*>(pipe->producer());
            if(controlable)
                lane_id = pipe->consumer()->get_id();
            else
                lane_id = pipe->producer()->get_id();
            if(occupied_memory.size() < lane_id)
                occupied_memory.resize(lane_id, 0);
            occupied_memory[lane_id] += pipe->bytes_used();
        }
        return occupied_memory;
    }
    void control_lane_diff_memory(uint64_t max_diff_memory = 2ull << 27) //29
    {

        const uint64_t EPSILON_MEMORY = 2ull << 27; //CHANGED FROM 27
        auto memory_per_lane = get_memory_per_lane();
        auto min_memory = std::min_element(memory_per_lane.begin(), memory_per_lane.end());
        auto max_memory = std::max_element(memory_per_lane.begin(), memory_per_lane.end());
        auto full_lane_index = 0;
        if (*max_memory - *min_memory > max_diff_memory)
        {
            full_lane_index = (max_memory - memory_per_lane.begin()) + 1;
        }
        for (auto & node : nodes_)
        {
            auto controlable = dynamic_cast<i_controlable_source*>(node.get());
            if(controlable != nullptr )
            {
                if(node->get_id() == full_lane_index)
                    controlable->pause_production();
                else
                    controlable->continue_production(); 
            }
                    
        }
    }
    bool is_done_ = false;
    void start_all()
    {
        is_done_ = false;
        for (uint32_t i = 0; i < nodes_.size(); i++)
        {
            threads_.emplace_back(std::move(std::thread([this, i](){nodes_[i]->start();})));
        }
        for(auto data_source : data_sources_)
        {
            data_source->store_controller(this);
        }
        threads_.emplace_back(std::move(std::thread([this](){
                while(!is_done_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    //control_memory_usage();
                    if(debug_mode_)
                        print_pipe_state();
                    if(data_sources_.size() > 1)
                        control_lane_diff_memory(); 
                }
            })));
    } 
    std::vector<std::string> results()
    {
        std::vector<std::string> results;
        for(const auto & node : nodes_)
        {
            auto result = node->result();
            if(result != "")
                results.push_back(result);
        }
        return results;
    }
    void wait_all()
    {

        for (uint32_t i = 0; i < nodes_.size(); i++)
        {
            threads_[i].join();  
        }
        is_done_ = true;
        threads_[threads_.size() - 1].join();
        threads_.clear();
        threads_.resize(0);

    }

    


};