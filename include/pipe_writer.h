#include "pipe.h"
#pragma once
template <typename data_type>
class pipe_writer
{
    pipe<data_type>* pipe_;
    public:
    pipe_writer(): pipe_(nullptr){}
    pipe_writer(pipe<data_type>* pipe):
    pipe_(pipe){}
    bool write_bulk(data_buffer<data_type> * data_buffer)
    {
        //while(pipe_->approx_size() > pipe<data_type>::MAX_Q_LEN);
        for (uint32_t i = 0; i < data_buffer->size(); i++)
        {
            pipe_->blocking_enqueue(std::move(data_buffer->data()[i]));
        }
        data_buffer->clear();
        return true;
    }
    bool write(data_type && hit)
    {
        if(pipe_ == nullptr)
            return false;
        pipe_->blocking_enqueue(std::move(hit));
        return true;
    }
};