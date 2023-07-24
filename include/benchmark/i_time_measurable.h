
#include "measuring_clock.h"

#pragma once
//an interface which defines a node that can measure its execution time
//what is measured depends on the node implementation
class i_time_measurable
{
public:
    virtual void prepare_clock(measuring_clock *clock) = 0; // recommended to be called before start method of a node
};