#include "i_data_consumer.h"
#include "pipe_reader.h"
#pragma once
//a simple consumer which reads the data from a single pipe
template <typename data_type>
class i_simple_consumer : public i_data_consumer<data_type>
{
protected:
    pipe_reader<data_type> reader_;

public:
    virtual ~i_simple_consumer(){};
    virtual void connect_input(default_pipe<data_type> *pipe) final override
    {
        reader_ = pipe_reader<data_type>(pipe);
    }
    
};