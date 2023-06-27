#include "pipe.h"
#pragma once

class i_data_node
{
    protected:
    uint32_t id = 1;
    public:

    uint32_t get_id()
    {
        return id;
    }
    void set_id(uint32_t new_id)
    {
        id = new_id;
    }
    virtual std::string name() = 0;
    virtual void start() = 0;

    virtual std::vector<i_data_node*> internal_nodes()
    {
        return std::vector<i_data_node*>();
    }
    virtual std::vector<abstract_pipe*> internal_pipes()
    {
        return std::vector<abstract_pipe*>();
    }
    virtual std::string result()
    {
        return "";
    }
    virtual ~i_data_node() = default;
};