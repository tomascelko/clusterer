#pragma once
class abstract_pipe
{
    public:
    virtual std::string name() = 0;
    virtual ~abstract_pipe(){};
    virtual uint64_t bytes_used() = 0;
};