#pragma once
class i_sync_node
{
    public:
    virtual void sync_time() = 0;
    virtual double check_time() = 0;
    virtual uint64_t queue_size() = 0;
    
    virtual void update_producer_time(double toa, uint32_t producer_id) = 0; //consumer informs that it received data from producer
    virtual double min_producer_time() = 0;//find last CONSUMED cluster by CONSUMER for each producer
    
};