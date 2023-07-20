class dataflow_controller;
#pragma once
class i_controlable_source
{
public:
    virtual void pause_production() = 0;
    virtual void continue_production() = 0;
    virtual void store_controller(dataflow_controller *controller) = 0;
};