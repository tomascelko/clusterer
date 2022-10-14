#pragma once
class abstract_pipe
{
    public:
    virtual ~abstract_pipe(){};
    virtual uint64_t bytes_used() = 0;
};