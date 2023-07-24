#include "pipe.h"
#pragma once
#include "pipe.h"
//writes the data to a single pipe
template <typename data_type>
class pipe_writer
{
    default_pipe<data_type> *pipe_;
    data_block<data_type> block_;

public:
    pipe_writer() : pipe_(nullptr), block_() {}
    pipe_writer(default_pipe<data_type> *pipe) : pipe_(pipe),
                                                 block_(pipe_->mean_data_size())
    {
    }
    default_pipe<data_type> *pipe()
    {
        return pipe_;
    }
    void set_block_params(uint32_t expected_size, double start_time)
    {
        block_ = data_block<data_type>(expected_size, start_time);
    }
    bool write_bulk(data_buffer<data_type> *data_buffer)
    {
        for (uint32_t i = 0; i < data_buffer->size(); i++)
        {
            write(std::move(data_buffer->data()[i]));
        }
        data_buffer->clear();
        return true;
    }
    bool write(data_type &&hit)
    {
        if (pipe_ == nullptr)
            return false;
        if (!block_.try_add_hit(std::move(hit)))
        {
            flush();
        }
        return true;
    }
    void flush()
    {
        if (pipe_ != nullptr)
        {
            double new_start_time;
            pipe_->blocking_enqueue(std::move(block_));
            block_.clear(pipe_->mean_data_size());
        }
    }
    void close()
    {
        write(data_type::end_token());
        flush();
    }
    virtual uint32_t open_pipe_count()
    {
        return 1u;
    }
};