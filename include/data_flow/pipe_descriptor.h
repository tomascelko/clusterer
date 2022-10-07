#include "../data_structs/cluster.h"
#include <cmath>
#pragma once
template <typename data_type>
class pipe_descriptor
{
protected:
    uint32_t pipe_count_;
public:
    uint32_t pipe_count()
    {
        return pipe_count_;
    }
    pipe_descriptor(uint32_t pipe_count) :
    pipe_count_(pipe_count){}
    virtual uint32_t get_pipe_index(const data_type & data) = 0;
    virtual bool is_on_border(const cluster<data_type> & cl) = 0;
    virtual ~pipe_descriptor() = default;
};

template <typename hit_type>
class temporal_clustering_descriptor : public pipe_descriptor<hit_type>
{
    const uint32_t SWITCH_INTERVAL_LEN = 4000; //in nanoseconds
    const double EPSILON_BORDER_TIME = 300; 
    uint32_t full_rotation_interval_;
public:
temporal_clustering_descriptor() : pipe_descriptor<hit_type>(1){}
    temporal_clustering_descriptor(uint32_t pipe_count) :
    pipe_descriptor<hit_type>(pipe_count),
    full_rotation_interval_(pipe_count * SWITCH_INTERVAL_LEN)
    {}
    virtual uint32_t get_pipe_index(const hit_type & hit) override
    {
        return (std::llround(hit.toa()) % full_rotation_interval_) / SWITCH_INTERVAL_LEN;
        
    }
    bool is_on_border(const cluster<hit_type> & cl) override
    {
        double first_remainder = std::llround(cl.first_toa()) % SWITCH_INTERVAL_LEN;
        double last_remainder = std::llround(cl.last_toa()) % SWITCH_INTERVAL_LEN;
        return first_remainder < EPSILON_BORDER_TIME  
            || first_remainder > SWITCH_INTERVAL_LEN - EPSILON_BORDER_TIME
            || last_remainder < EPSILON_BORDER_TIME   
            || last_remainder > SWITCH_INTERVAL_LEN - EPSILON_BORDER_TIME
            || cl.last_toa() - cl.first_toa() > SWITCH_INTERVAL_LEN;
    }
    virtual ~temporal_clustering_descriptor() = default;

};