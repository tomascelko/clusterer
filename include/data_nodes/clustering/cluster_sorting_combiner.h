#include "../../data_flow/dataflow_package.h"
#include "../../data_structs/cluster.h"
#include "../../utils.h"
#include "../../devices/current_device.h"
#include "../../benchmark/i_time_measurable.h"
#pragma once
template <typename hit_type>
class cluster_sorting_combiner : public i_data_consumer<cluster<hit_type>>, 
    public i_simple_producer<cluster<hit_type>>
{
    multi_cluster_pipe_reader<hit_type> reader_;
    public:
    void connect_input(default_pipe<cluster<hit_type>>* input_pipe) override
    {
        reader_.add_pipe(input_pipe);
    }
    
    std::string name() override
    {
        return "cluster_sorting_combiner";
    }
    virtual void start() override
    {
        cluster<hit_type> cl;
        uint64_t processed = 0;
        this->reader_.read(cl);
        while(cl.is_valid())
        {
            this->writer_.write(std::move(cl));
            ++processed; 
            this->reader_.read(cl);

        }
        this->writer_.write(cluster<hit_type>::end_token());
        this->writer_.flush();
        std::cout << name() << " ENDED ----------------" << std::endl;
    }

    virtual ~cluster_sorting_combiner() = default;
};