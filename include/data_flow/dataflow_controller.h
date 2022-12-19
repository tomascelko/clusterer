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
#include "../data_structs/cluster.h"
#include "../data_structs/mm_hit.h"
#include "../data_structs/burda_hit.h"
class dataflow_controller
{
    using node_pointer = std::unique_ptr<i_data_node>;
    using pipe_pointer = std::unique_ptr<abstract_pipe>;
private:
    std::vector<node_pointer> nodes_;
    //TODO add map<string, node_pointer> ktora bude mapovat meno na uzol
    std::vector<pipe_pointer> pipes_;
    std::vector<std::thread> threads_;
    uint32_t memory_control_counter_ = 0;
    std::ostream& state_info_out_stream_ = std::cout;
    void register_node(i_data_node* item)
    {
        nodes_.emplace_back(std::move(std::unique_ptr<i_data_node>(item)));
    }
    //template <typename data_type>
    void register_pipe(abstract_pipe* pipe)
    {
        pipes_.emplace_back(std::move(std::unique_ptr<abstract_pipe>(pipe)));
    }
public:
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
    }

    template <typename data_type>
    default_pipe<data_type>* connect_nodes(i_data_producer<data_type>* producer, i_data_consumer<data_type>* consumer)
    {
        default_pipe<data_type>* connecting_pipe = new default_pipe<data_type>(producer->name() + "__&__" + 
            consumer->name() + "_" + std::to_string(pipes_.size()));
        return connect_nodes<data_type>(producer, consumer, connecting_pipe);


    }
    
    void connect_nodes(i_data_node* producer, i_data_node* consumer)
    {
        //TODO create workflow where we can find 
        if(dynamic_cast<i_data_producer<mm_hit>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<mm_hit>*>(producer), dynamic_cast<i_data_consumer<mm_hit>*>(consumer));
        else if(dynamic_cast<i_data_producer<burda_hit>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<burda_hit>*>(producer), dynamic_cast<i_data_consumer<burda_hit>*>(consumer));
        else if (dynamic_cast<i_data_producer<cluster<mm_hit>>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<cluster<mm_hit>>*>(producer), dynamic_cast<i_data_consumer<cluster<mm_hit>>*>(consumer));
        else if (dynamic_cast<i_data_producer<cluster<burda_hit>>*>(producer) != nullptr)
            connect_nodes(dynamic_cast<i_data_producer<cluster<burda_hit>>*>(producer), dynamic_cast<i_data_consumer<cluster<burda_hit>>*>(consumer));
        else
            throw std::invalid_argument("provided data node produces unsupported data type");
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
    void check_pipe_state()
    {
        for (auto & pipe : pipes_)
        {
            state_info_out_stream_ << pipe->name() << " used MB:" << pipe->bytes_used()/(2<<(20-1)) << std::endl;
        }
        state_info_out_stream_ << "----------------" << std::endl;
    }
    void control_memory_usage(uint64_t max_memory = 2ull << 29) //29
    {
        uint64_t used_memory = get_used_memory();
        const uint64_t EPSILON_MEMORY = 2ull << 27; //CHANGED FROM 27
        memory_control_counter_ ++;
        //if(memory_control_counter_ % 2 == 0)
        //   state_info_out_stream_ << used_memory / 1000000. << " MB" << std::endl;
        for (auto & node : nodes_)
        {
            auto controlable = dynamic_cast<i_controlable_source*>(node.get());
            if(controlable != nullptr)
            {
                if(used_memory > max_memory)
                {
                    controlable->pause_production();
                }
                if(used_memory < max_memory - EPSILON_MEMORY)
                    {
                    controlable->continue_production();
                    
                    }
            }
            
        }
    }
    bool is_done_ = false;
    void start_all()
    {
        for (uint32_t i = 0; i < nodes_.size(); i++)
        {
            threads_.emplace_back(std::move(std::thread([this, i](){nodes_[i]->start();})));
        }
        threads_.emplace_back(std::move(std::thread([this](){
                while(!is_done_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    control_memory_usage();
                    check_pipe_state();
                }
            })));
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

    }

    


};