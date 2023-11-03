class dataflow_controller;
#pragma once
//interface which defines a node which produces data and can be controlled from outside
//additionally, it can use the controller to control itself
class i_controlable_source
{
public:
    virtual void pause_production() = 0;
    virtual void continue_production() = 0;
    virtual uint64_t get_total_hit_count() = 0;
    virtual void store_controller(dataflow_controller *controller) = 0;
};