#include "pipe.h"
#pragma once
template <typename data_type>
class pipe_reader
{
    pipe<data_type>* pipe_;
    public:
    pipe_reader(){}
    pipe_reader(pipe<data_type>* pipe):
    pipe_(pipe){}
    bool read(data_type & hit)
    {
        pipe_->blocking_dequeue(hit);
        return true;
    }
};