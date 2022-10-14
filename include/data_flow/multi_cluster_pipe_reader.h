#include "pipe.h"
#pragma once
template <typename hit_type>
class multi_cluster_pipe_reader
{
    using reader_type = pipe_reader<cluster<hit_type>>;
    std::vector<reader_type> readers_;
    std::vector<double> last_toas_;
    std::vector<cluster<hit_type>> cluster_buffer_;
    uint32_t reading_index_;
    uint64_t bytes_read_ = 0;
    bool is_initiating_phase_ = true;
    public:
    multi_cluster_pipe_reader(const std::vector<default_pipe<cluster<hit_type>>*> & pipes)
    {
        readers_.reserve(pipes.size());
        cluster_buffer_.reserve(pipes.size());
        for (auto pipe : pipes)
        {
            readers_.emplace_back(reader_type{pipe});
        }
    }
    multi_cluster_pipe_reader(){}
    uint64_t bytes_read()
    {
        return bytes_read_;
    }
    void add_pipe(default_pipe<cluster<hit_type>>* pipe)
    {
        readers_.emplace_back(reader_type{pipe});
    }
    void initial_read()
    {
        cluster<hit_type> temp_cl;
        double min_toa = LONG_LONG_MAX;
        for(uint32_t i = 0; i < readers_.size(); ++i)
        {
            readers_[i].read(temp_cl);
            cluster_buffer_.emplace_back(std::move(temp_cl));
        }
        is_initiating_phase_ = false;
    }
    bool read(cluster<hit_type> & cl)
    {
        if(is_initiating_phase_)
            initial_read();
        double min_toa = LONG_LONG_MAX;
        uint32_t argmin_toa = -1;
        for(uint32_t i = 0; i < cluster_buffer_.size(); ++i)
        {
            if(cluster_buffer_[i].is_valid() && cluster_buffer_[i].first_toa() < min_toa)
            {
                min_toa = cluster_buffer_[i].first_toa();
                argmin_toa = i;
            }
        }
        if(argmin_toa == -1)
            cl = std::move(cluster_buffer_[0]); //we found all of the end tokens
        else
        {
            cl = std::move(cluster_buffer_[argmin_toa]);
            readers_[argmin_toa].read(cluster_buffer_[argmin_toa]);
        }
        bytes_read_ += cl.size();
        return true;


    }
};
//TODO multi cluster pipe writer