#include "../data_flow/dataflow_package.h"

template <typename data_type, typename stream_type>
class data_printer : public i_data_consumer<data_type>, public i_data_producer<data_type>
{
    pipe_reader<data_type> reader_;
    pipe_writer<data_type> writer_;
    std::unique_ptr<stream_type> out_stream_;
    public:
    data_printer(stream_type* print_stream) :
    out_stream_(std::unique_ptr<stream_type>(print_stream))
    {
        //out_stream_ = std::move(std::make_unique<std::ostream>(print_stream));
    }
    virtual void start() override
    {
        data_type hit;
        uint64_t processed = 0;
        reader_.read(hit);
        while(hit.is_valid())
        {
            writer_.write(std::move(hit));
            ++processed; 
            (*out_stream_) << hit;
            reader_.read(hit);

        }
        out_stream_->close();
        std::cout << "PRINTER ENDED ----------------" << std::endl;
    }
    virtual void connect_input(pipe<data_type>* in_pipe) override
    {
        reader_ = pipe_reader<data_type>(in_pipe);
    }
    virtual void connect_output(pipe<data_type>* out_pipe) override
    {
        writer_ = pipe_writer<data_type>(out_pipe);
    }
    virtual ~data_printer() = default;
};