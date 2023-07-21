#include "pipe.h"
#include "i_data_node.h"
#pragma once
//an interface used for connecting the data producer with other nodes
template <typename data_type>
class i_data_producer : virtual public i_data_node
{
public:
    virtual ~i_data_producer(){};
    virtual void connect_output(default_pipe<data_type> *pipe) = 0;
};