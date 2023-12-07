#include "i_simple_producer.h"
#pragma once
//and example implementation of a data producer capable of splitting the data based on
//the split descriptor
template <typename data_type>
class i_multi_producer : public i_data_producer<data_type>
{
protected:
    multi_pipe_writer<data_type> writer_;

public:
    i_multi_producer(split_descriptor<data_type> *split_descriptor) : writer_(split_descriptor){};
    i_multi_producer() : writer_(){};
    virtual ~i_multi_producer(){};
    virtual void connect_output(default_pipe<data_type> *pipe) final override
    {
        writer_.add_pipe(pipe);
    }
};