#pragma once
class i_data_node;
class abstract_pipe
{
public:
    virtual std::string name() = 0;
    virtual ~abstract_pipe(){};
    virtual uint64_t bytes_used() = 0;
    virtual i_data_node *producer() = 0;
    virtual i_data_node *consumer() = 0;
};