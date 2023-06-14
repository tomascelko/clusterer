#include "../data_structs/cluster.h"
#include <cmath>
#pragma once
class abstract_pipe_descriptor
{
    public:
    virtual uint32_t pipe_count() = 0;
    virtual ~abstract_pipe_descriptor() = default;
};

template <typename data_type>
class split_descriptor : public virtual abstract_pipe_descriptor
{
    public:
    virtual uint32_t get_pipe_index(const data_type & data) = 0;
    virtual ~split_descriptor() = default;
};

template <typename data_type>
class merge_descriptor : public virtual abstract_pipe_descriptor
{
    public:
    virtual bool is_on_border(const data_type & data) = 0;
    virtual bool should_be_forwarded(const data_type & data) = 0;
    virtual ~merge_descriptor() = default;
};

class abstract_node_descriptor
{
    std::string descr_id;
    public:
    abstract_node_descriptor(const std::string & descr_id = "default_descr_id") :
    descr_id(descr_id){};
    virtual ~abstract_node_descriptor() = default;
};

template<typename merge_type, typename split_type>
struct node_descriptor : public abstract_node_descriptor
{
    merge_descriptor<merge_type>* merge_descr;
    split_descriptor<split_type> *split_descr;
    node_descriptor(merge_descriptor<merge_type> * merge_descr,  split_descriptor<split_type> * split_descr,
        const std::string & descr_id) :
    merge_descr(merge_descr),
    split_descr(split_descr)
    {}
    virtual ~node_descriptor()
    {
        delete merge_descr;
        delete split_descr;
    }

};

template <typename merge_type, typename split_type>
class merge_split_descriptor : public split_descriptor<split_type>,
    public  merge_descriptor<merge_type>
{
protected:
    uint32_t pipe_count_;
    public:

    uint32_t pipe_count() override
    {
        return pipe_count_;
    }

    merge_split_descriptor(uint32_t pipe_count) :
    pipe_count_(pipe_count)
    {}
    
    virtual ~merge_split_descriptor() = default;
};

template <typename hit_type>
class temporal_clustering_descriptor : public merge_split_descriptor<cluster<hit_type>, hit_type>
{
    const uint32_t SWITCH_INTERVAL_LEN = 8000; //in nanoseconds
    const double EPSILON_BORDER_TIME = 300; 
    uint32_t full_rotation_interval_;
    double offset_ = 0;
public:
temporal_clustering_descriptor() : merge_split_descriptor<cluster<hit_type>, hit_type>(2){}
    temporal_clustering_descriptor(uint32_t pipe_count) :
    merge_split_descriptor<cluster<hit_type>, hit_type>(pipe_count),
    full_rotation_interval_(pipe_count * SWITCH_INTERVAL_LEN)
    {}
    uint32_t get_pipe_index(const hit_type & hit) override
    {
        return (std::llround(hit.toa()) % full_rotation_interval_) / SWITCH_INTERVAL_LEN;
        
    }
    bool is_on_border(const cluster<hit_type> & cl) override
    {
        const uint32_t depth = 1;
        double first_remainder = std::abs((std::llround(cl.first_toa()) % (SWITCH_INTERVAL_LEN)) - offset_);
        double last_remainder = std::abs((std::llround(cl.last_toa()) % SWITCH_INTERVAL_LEN) - offset_);
        return first_remainder < EPSILON_BORDER_TIME  
            || first_remainder > SWITCH_INTERVAL_LEN - EPSILON_BORDER_TIME
            || last_remainder < EPSILON_BORDER_TIME   
            || last_remainder > SWITCH_INTERVAL_LEN - EPSILON_BORDER_TIME
            || cl.last_toa() - cl.first_toa() > SWITCH_INTERVAL_LEN;
    }
    bool should_be_forwarded(const cluster<hit_type> & cl) override
    {
        return false;
    }

    virtual ~temporal_clustering_descriptor() = default;

};


template <typename data_type>
class trivial_split_descriptor : public split_descriptor<data_type>
{
    public:
    trivial_split_descriptor(){}
    uint32_t get_pipe_index(const data_type & hit) override
    {
        return 0;
    }
    uint32_t pipe_count() override
    {
        return 1;
    }
    virtual ~trivial_split_descriptor() = default;
};


template <typename data_type>
class temporal_hit_split_descriptor : public split_descriptor<data_type>
{
    uint32_t pipe_count_;
    const uint32_t SWITCH_INTERVAL_LEN = 8000; //in nanoseconds
    const double EPSILON_BORDER_TIME = 300; 
    uint32_t full_rotation_interval_;
    public:
    temporal_hit_split_descriptor(uint32_t pipe_count):
    pipe_count_(pipe_count),
    full_rotation_interval_(pipe_count * SWITCH_INTERVAL_LEN)
    {}
    uint32_t get_pipe_index(const data_type & hit) override
    {
        return (std::llround(hit.toa()) % full_rotation_interval_) / SWITCH_INTERVAL_LEN;
        
    }
    uint32_t pipe_count() override
    {
        return pipe_count_;
    }
    virtual ~temporal_hit_split_descriptor() = default;
};

template <typename data_type>
class trivial_merge_descriptor : public merge_descriptor<data_type>
{
    public:
    trivial_merge_descriptor()
    {}
    bool is_on_border(const data_type & cl) override
    {
        return false;
    }
    bool should_be_forwarded(const data_type & cl) override
    {
        return false;
    }
    uint32_t pipe_count() override
    {
        return 1;
    }
    virtual ~trivial_merge_descriptor() = default;
};



template <typename cluster_type>
class temporal_merge_descriptor : public merge_split_descriptor<cluster_type, cluster_type>
{
    const uint32_t SWITCH_INTERVAL_LEN = 8000; //in nanoseconds
    const double EPSILON_BORDER_TIME = 300; 
    uint32_t full_rotation_interval_;
    uint32_t depth_;
    bool inner_node_;
    bool is_on_border_multiple(const cluster_type & cl, uint32_t multiple)
    {
        uint32_t switch_interval_len = multiple * depth_ * SWITCH_INTERVAL_LEN;
        double first_remainder = std::abs((std::llround(cl.first_toa()) % (switch_interval_len)));
        double last_remainder = std::abs((std::llround(cl.last_toa()) % switch_interval_len));
        return first_remainder < EPSILON_BORDER_TIME  
            || first_remainder > switch_interval_len - EPSILON_BORDER_TIME
            || last_remainder < EPSILON_BORDER_TIME   
            || last_remainder > switch_interval_len - EPSILON_BORDER_TIME
            || cl.last_toa() - cl.first_toa() > switch_interval_len;
    }
public:
    temporal_merge_descriptor(uint32_t pipe_count, uint32_t depth, bool inner_node = true) :
    merge_split_descriptor<cluster_type, cluster_type>(pipe_count),
    depth_(depth),
    inner_node_(inner_node),
    full_rotation_interval_(pipe_count * SWITCH_INTERVAL_LEN)
    {}
    uint32_t get_pipe_index(const cluster_type & cl) override
    {
        if(inner_node_ && should_be_forwarded(cl))
            return 1; 
        return 0;
        
    }
    bool is_on_border(const cluster_type & cl) override
    {
        return is_on_border_multiple(cl, 1);
    }
    bool should_be_forwarded(const cluster_type & cl) override
    {
        return inner_node_ && is_on_border_multiple(cl, 2);
    }
    virtual ~temporal_merge_descriptor() = default;

};

template <typename cl_type>
class clustering_two_split_descriptor : public split_descriptor<cl_type>
{
    const uint32_t SWITCH_INTERVAL_LEN = 8000; //in nanoseconds
    const double EPSILON_BORDER_TIME = 300;
    public:
    uint32_t get_pipe_index(const cl_type & cl) override
    {
        uint32_t remainder = std::abs(std::llround(cl.first_toa()) % SWITCH_INTERVAL_LEN);
        if (remainder < SWITCH_INTERVAL_LEN / 2)
            return 0;
        return 1;
    }
    uint32_t pipe_count() override
    {
        return 2;
    }
    virtual ~clustering_two_split_descriptor() = default;
};
