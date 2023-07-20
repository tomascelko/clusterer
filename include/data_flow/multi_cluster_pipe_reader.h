#include "pipe.h"
#pragma once
template <typename data_type>
class multi_pipe_reader
{
    using reader_type = pipe_reader<data_type>;
    std::vector<reader_type> readers_;
    std::vector<data_type> cluster_buffer_;
    uint32_t reading_index_;
    uint64_t bytes_read_ = 0;
    bool is_initiating_phase_ = true;

public:
    multi_pipe_reader(const std::vector<default_pipe<data_type> *> &pipes)
    {
        readers_.reserve(pipes.size());
        cluster_buffer_.reserve(pipes.size());
        for (auto pipe : pipes)
        {
            readers_.emplace_back(reader_type{pipe});
        }
    }
    multi_pipe_reader() {}
    uint64_t bytes_read()
    {
        return bytes_read_;
    }
    uint32_t last_read_pipe()
    {
        return reading_index_;
    }
    void add_pipe(default_pipe<data_type> *pipe)
    {
        readers_.emplace_back(reader_type{pipe});
    }

    bool read(data_type &cl)
    {
        double min_toa = std::numeric_limits<double>::max();
        uint32_t argmin_toa = -1;

        for (uint32_t i = 0; i < readers_.size(); ++i)
        {
            const data_type &current = readers_[i].peek();
            if (current.is_valid() && current.time() < min_toa)
            {
                min_toa = current.time();
                argmin_toa = i;
            }
        }
        if (argmin_toa == -1)
        {
            readers_[0].read(cl); // we found all of the end tokens
            reading_index_ = 0;
        }
        else
        {
            readers_[argmin_toa].read(cl);
            reading_index_ = argmin_toa;
        }
        bytes_read_ += cl.size();
        return true;
    }
};
// TODO multi cluster pipe writer