#include "../../data_flow/dataflow_package.h"
//print the data type to a file using << operator
template <typename data_type, typename stream_type>
class data_printer : public i_data_consumer<data_type>, public i_simple_producer<data_type>
{
    std::unique_ptr<stream_type> out_stream_;
    multi_pipe_reader<data_type> reader_;

public:
    data_printer(stream_type *print_stream) : out_stream_(std::unique_ptr<stream_type>(print_stream))
    {
        // out_stream_ = std::move(std::make_unique<std::ostream>(print_stream));
    }
    std::string name() override
    {
        return "printer";
    }
    void connect_input(default_pipe<data_type> *input_pipe) override
    {
        reader_.add_pipe(input_pipe);
    }
    virtual void start() override
    {
        data_type hit;
        uint64_t processed = 0;
        this->reader_.read(hit);
        while (hit.is_valid())
        {
            this->writer_.write(std::move(hit));
            ++processed;
            (*out_stream_) << hit;
            this->reader_.read(hit);
        }
        out_stream_->close();
        this->writer_.close();
    }

    virtual ~data_printer() = default;
};