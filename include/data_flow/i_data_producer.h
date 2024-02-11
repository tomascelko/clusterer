#include "pipe.h"
#include "i_data_node.h"
#include "../data_structs/cluster.h"
#include "../utils.h"
#pragma once
//an interface used for connecting the data producer with other nodes
template <typename data_type>
class i_data_producer : virtual public i_data_node
{
protected:
    static constexpr bool is_cluster = is_instance_of_template<data_type,cluster>::value;
    uint64_t newly_processed_count(const data_type & data)
    {
        return get_processed_count<is_cluster, data_type>(data);
    }
public:
    virtual ~i_data_producer(){};
    virtual void connect_output (default_pipe<data_type> *pipe) = 0;
    
};