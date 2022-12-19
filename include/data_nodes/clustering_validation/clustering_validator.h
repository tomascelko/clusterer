#include "../../data_flow/dataflow_package.h"

template <typename hit_type, typename stream_type>
class data_printer : public i_data_consumer<cluster<data_type>>
{
    std::unique_ptr<stream_type> out_stream_;
    public:
    data_printer() :
    out_stream_(std::unique_ptr<stream_type>(print_stream))
    {
        //out_stream_ = std::move(std::make_unique<std::ostream>(print_stream));
    }
    std::string name() override
    {
        return "clustering IoU validator";
    }
    void connect_input(default_pipe<cluster<hit_type>>* input_pipe) override
    {
        reader_.add_pipe(input_pipe);
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