#include <vector>
#include <memory>
#include "i_data_node.h"
#include "i_data_consumer.h"
#include "i_data_producer.h"
#include "pipe.h"
#include "abstract_pipe.h"
#include <thread>
class dataflow_controller
{
    using node_pointer = std::unique_ptr<i_data_node>;
    using pipe_pointer = std::unique_ptr<abstract_pipe>;
private:
    std::vector<node_pointer> nodes_;
    //TODO add map<string, node_pointer> ktora bude mapovat meno na uzol
    std::vector<pipe_pointer> pipes_;
    std::vector<std::thread> threads_;
    void register_node(i_data_node* item)
    {
        nodes_.emplace_back(std::move(std::unique_ptr<i_data_node>(item)));
    }
    template <typename data_type>
    void register_pipe(pipe<data_type>* pipe)
    {
        pipes_.emplace_back(std::move(std::unique_ptr<abstract_pipe>(pipe)));
    }
public:
        
    void remove_all_nodes()
    {
        nodes_.clear();
    }

    template <typename data_type>
    pipe<data_type>* connect_nodes(i_data_producer<data_type>* producer, i_data_consumer<data_type>* consumer)
    {
        pipe<data_type>* connecting_pipe = new pipe<data_type>(pipes_.size());
        return connect_nodes<data_type>(producer, consumer, connecting_pipe);


    }
    void add_node(i_data_node * data_node)
    {
        //todo save map with name of node
        register_node(data_node);
    }
    template <typename data_type>
    pipe<data_type>* connect_nodes(i_data_producer<data_type>* producer, i_data_consumer<data_type>* consumer, pipe<data_type>* pipe)
    {         
        register_pipe<data_type>(pipe);
        producer->connect_output(pipe);
        consumer->connect_input(pipe);
        return pipe;
    }
    void start_all()
    {
        for (uint32_t i = 0; i < nodes_.size(); i++)
        {
            threads_.emplace_back(std::move(std::thread([this, i](){nodes_[i]->start();})));
        }
        for (uint32_t i = 0; i < nodes_.size(); i++)
        {
            threads_[i].join();
        }
    } 
    void wait_all()
    {

    }

    


};