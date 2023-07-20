// obtains feaure vectors (including LABELS) and performs quantization
// at the end send all data

#include "../../data_flow/dataflow_package.h"
#include "default_window_feature_vector.h"
template <typename feature_vect_type>
class lvq_quantizer : public i_simple_consumer<feature_vect_type>, public i_simple_producer<feature_vect_type>
{
    default_window_state window_state_;

public:
    window_feature_computer()
    {
    }
    std::string name() override
    {
        return "feature_computer";
    }
    virtual void start() override
    {
        data_type hit;
        uint64_t processed = 0;
        this->reader_.read(hit);
        while (hit.is_valid())
        {
            window_state_.update(hit) if (window_state_.is_end())
            {
                this->writer_.write(window_state_.to_feature_vector())
                    window_state.reset();
            }
            ++processed;

            this->reader_.read(hit);
        }
        out_stream_->close();
        // std::cout << "FEATURE COMPUTER ENDED ----------------" << std::endl;
    }

    virtual ~data_printer() = default;
};