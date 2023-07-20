// obtains sorted mm hits
// returns vector of window features

#include "../../data_flow/dataflow_package.h"
#include "window_state.h"
template <typename feature_vect_type, typename data_type, typename window_state = default_window_state<data_type>>
class window_feature_computer : public i_simple_consumer<data_type>, public i_simple_producer<feature_vect_type>
{
    window_state window_state_;

public:
    window_feature_computer(const node_args &args) : window_state_(args.get_arg<double>(name(), "window_size"), args.get_arg<double>(name(), "diff_window_size"))
    {
    }
    std::string name() override
    {
        return "window_computer";
    }
    double max_frequency_ = 0;
    std::string result() override
    {
        return std::to_string(max_frequency_);
    }
    virtual void start() override
    {
        data_type hit;
        uint64_t processed = 0;
        this->reader_.read(hit);
        while (hit.is_valid())
        {

            if (window_state_.can_add(hit))
            {
                window_state_.update(hit);
            }
            else
            {
                uint32_t empty_count = window_state_.get_empty_count(hit);
                if (window_state_.mean_frequency() > max_frequency_)
                    max_frequency_ = window_state_.mean_frequency();
                this->writer_.write(window_state_.to_feature_vector());
                window_state_.move_window();
                for (uint32_t i = 0; i < empty_count; i++)
                {
                    this->writer_.write(window_state_.to_feature_vector());
                    window_state_.move_window();
                    window_state_.update(hit);
                }
            }
            ++processed;

            this->reader_.read(hit);
        }
        this->writer_.write(feature_vect_type::end_token());
        this->writer_.flush();
        std::cout << "FEATURE COMPUTER ENDED ----------------" << std::endl;
    }

    virtual ~window_feature_computer() = default;
};