#include "pipe.h"
#pragma once
#include "pipe.h"
template <typename data_type>
class pipe_writer
{
    default_pipe<data_type>* pipe_;
    data_block<data_type> block_;
    public:

    pipe_writer(): pipe_(nullptr), block_(){}
    pipe_writer(default_pipe<data_type>* pipe):
    pipe_(pipe),
    block_()
    {}
    bool write_bulk(data_buffer<data_type> * data_buffer)
    {
        //while(pipe_->approx_size() > default_pipe<data_type>::MAX_Q_LEN);
        for (uint32_t i = 0; i < data_buffer->size(); i++)
        {
            write(std::move(data_buffer->data()[i]));
        }
        data_buffer->clear();
        return true;
    }
    bool write(data_type && hit)
    {
        if(pipe_ == nullptr)
            return false;
        if(!block_.try_add_hit(std::move(hit)))
        {
            flush();
        }
        return true;
    }
    void flush()
    {
        if (pipe_ != nullptr)
        {
            pipe_->blocking_enqueue(std::move(block_));
            block_.clear(); 
        }
    }
};