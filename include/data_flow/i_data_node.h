#include "pipe.h"
#pragma once

class i_data_node
{
    public:
    virtual void start() = 0;
    
    virtual std::vector<i_data_node*> internal_nodes()
    {
        return std::vector<i_data_node*>();
    }
    virtual std::vector<abstract_pipe*> internal_pipes()
    {
        return std::vector<abstract_pipe*>();
    }
    virtual ~i_data_node() = default;
};