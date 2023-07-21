#include "pipe.h"
#include "i_data_node.h"
#pragma once
//an interface used for connecting a consumer node
//can be overriden for single or multi input nodes
template <typename data_type>
class i_data_consumer : virtual public i_data_node
{
public:
    virtual ~i_data_consumer(){};
    virtual void connect_input(default_pipe<data_type> *pipe) = 0;
};
