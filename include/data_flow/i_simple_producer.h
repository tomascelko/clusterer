#include "i_data_producer.h"
#include "pipe_writer.h"
#pragma once
//a simple producer which produces all data to a single pipe
template <typename data_type>
class i_simple_producer : public i_data_producer<data_type>
{
protected:
    pipe_writer<data_type> writer_;

public:
    virtual ~i_simple_producer(){};
    virtual void connect_output(default_pipe<data_type> *pipe) final override
    {
        writer_ = pipe_writer<data_type>(pipe);

    }
};