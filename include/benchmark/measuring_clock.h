#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_package.h"
#pragma once
//stores the result of the single run of the model
class exec_time_result
{
private:
    std::string node_name_;
    double exec_time_;

public:
    double exec_time()
    {
        return exec_time_;
    }
    const std::string &node_name()
    {
        return node_name_;
    }
    exec_time_result(const std::string &name, double time) : node_name_(name),
                                                             exec_time_(time)
    {
    }
};
//wraps the implementation of a clock with the capability of containing a callback
//which is called after the model was executed 
class measuring_clock
{
    bool measure_;
    using clock_type = std::chrono::steady_clock;

    std::chrono::duration<double> duration_ = std::chrono::duration<double>(0.);
    std::chrono::time_point<clock_type> start_point_;
    std::function<void(exec_time_result)> finish_callback_;

public:
    //begins the run of the clock
    void start()
    {
        if (measure_)
        {
            start_point_ = clock_type::now();
        }
    }
    //pauses the run of the clock
    void pause()
    {
        if (measure_)
        {
            duration_ += std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - start_point_);
        }
    }
    //returns the elapsed time 
    //only call after "stop and report" was called 
    std::chrono::duration<double> elapsed_time()
    {
        return duration_;
    }
    //pauses the clock and calls the provided callback to report the result
    void stop_and_report(const std::string node_id)
    {
        duration_ += std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - start_point_);
        finish_callback_(exec_time_result(node_id, duration_.count()));
    }
    measuring_clock(std::function<void(exec_time_result)> finish_callback, bool measure = false) : finish_callback_(finish_callback),
                                                                                                   measure_(measure)
    {
    }
};