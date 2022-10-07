
#include "measuring_clock.h"

#pragma once
class i_time_measurable
{
    public:
    virtual void prepare_clock(measuring_clock* clock) = 0; //recommended to be called before start method of a node
    


};