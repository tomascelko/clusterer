#include "pipe.h"
#include "i_data_node.h"
#pragma once
template <typename data_type>
class i_data_consumer : virtual public i_data_node
{
    // TODO make dataflow controller a friend so only it manipulate input
public:
    virtual ~i_data_consumer(){};
    virtual void connect_input(default_pipe<data_type> *pipe) = 0;
};
