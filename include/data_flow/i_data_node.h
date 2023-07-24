#include "pipe.h"
#pragma once
//interface defines base properties of the node
class i_data_node
{
protected:
    //identifier
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
    //node name (type)
    virtual std::string name() = 0;
    //start the run of a node
    virtual void start() = 0;

    virtual std::vector<i_data_node *> internal_nodes()
    {
        return std::vector<i_data_node *>();
    }
    virtual std::vector<abstract_pipe *> internal_pipes()
    {
        return std::vector<abstract_pipe *>();
    }
    //return result of the node computation converted to string, if any
    virtual std::string result()
    {
        return "";
    }
    virtual ~i_data_node() = default;
};