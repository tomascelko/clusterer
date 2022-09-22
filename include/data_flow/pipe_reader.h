#include "pipe.h"
#pragma once
template <typename data_type>
class pipe_reader
{
    pipe<data_type>* pipe_;
    uint32_t BLOCK_SIZE = 2<<8;
    data_block<data_type> block_;
    public:
    pipe_reader(): block_(BLOCK_SIZE){}
    pipe_reader(pipe<data_type>* pipe):
    pipe_(pipe),
    block_(BLOCK_SIZE){}
    bool read(data_type & hit)
    {
        while(!block_.try_remove_hit(hit))
        {
            pipe_->blocking_dequeue(block_);

        }
        return true;
    }
};   