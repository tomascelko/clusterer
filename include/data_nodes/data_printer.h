#include "../data_flow/dataflow_package.h"

template <typename data_type, typename stream_type>
class data_printer : public i_simple_consumer<data_type>, public i_simple_producer<data_type>
{
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
        this->reader_.read(hit);
        while(hit.is_valid())
        {
            this->writer_.write(std::move(hit));
            ++processed; 
            (*out_stream_) << hit;
            this->reader_.read(hit);

        }
        out_stream_->close();
        std::cout << "PRINTER ENDED ----------------" << std::endl;
    }

    virtual ~data_printer() = default;
};