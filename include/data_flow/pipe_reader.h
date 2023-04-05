#include "pipe.h"
#pragma once
template <typename data_type>
class pipe_reader
{
    default_pipe<data_type>* pipe_;
    data_block<data_type> block_;
    public:
    pipe_reader(): block_(){}
    pipe_reader(default_pipe<data_type>* pipe):
    pipe_(pipe),
    block_(){}
    bool read(data_type & hit)
    {
        while(!block_.try_remove_hit(hit))
        {
            pipe_->blocking_dequeue(block_);

        }
        return true;
    }
    bool is_empty()
    {
        return !block_.can_peek();
    }
    const data_type & peek()
    {
        if(!block_.can_peek())
            pipe_->blocking_dequeue(block_);
        return block_.peek();
        
    }
};   