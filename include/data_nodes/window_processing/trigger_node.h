#include "../../data_flow/dataflow_package.h"
#include "window_state.h"
#include "default_window_feature_vector.h"

#include "triggers.h"
template <typename data_type, typename window_state = default_window_state<data_type>>
class trigger_node : public i_simple_consumer<data_type>, public i_multi_producer<data_type>
{
    // TODO EVERYTHING
    window_state window_state_;
    using trigger_type = abstract_window_trigger<data_type, default_window_feature_vector<data_type>>;
    std::unique_ptr<trigger_type> trigger_;
    bool triggered_ = false;
    std::deque<data_type> hit_buffer_;
    double window_size_;
    uint64_t discarded_hit_count_ = 0;

    void initialize_trigger(const node_args &args)
    {
        const std::string INTERVAL_SUFFIX = ".ift";
        const std::string NN_SUFFIX = ".nnt";
        const std::string SVM_SUFFIX = ".svmt";
        const std::string OSVM_SUFFIX = ".osvmt";
        std::string trigger_file = args.get_arg<std::string>(name(), "trigger_file");
        std::function<bool(double)> default_trigger_func = [](double trigger_probability)
        {
            return trigger_probability > 0.5;
        };
        if (ends_with(trigger_file, INTERVAL_SUFFIX))
            trigger_ = std::move(std::make_unique<interval_trigger<data_type>>(args.at(name())));
        else if (ends_with(trigger_file, NN_SUFFIX))
        {
            trigger_ = std::move(std::make_unique<onnx_trigger<data_type, float>>(args.at(name()),
                                                                                  default_trigger_func));
        }
        else if (ends_with(trigger_file, SVM_SUFFIX))
        {
            std::function<bool(int)> trigger_func = [](int should_trigger)
            {
                return should_trigger > 0.5;
            };
            trigger_ = std::move(std::make_unique<onnx_trigger<data_type, int>>(args.at(name()),
                                                                                trigger_func));
        }
        else if (ends_with(trigger_file, OSVM_SUFFIX))
        {
            std::function<bool(int)> trigger_func = [](int outlier_mark)
            {
                return outlier_mark < 0;
            };
            trigger_ = std::move(std::make_unique<onnx_trigger<data_type, int>>(args.at(name()),
                                                                                trigger_func));
        }
        else
            throw std::invalid_argument("unsupperted trigger file passed - '" + trigger_file + "'");
    }

public:
    trigger_node(const node_args &args) : window_state_(args.get_arg<double>(name(), "window_size"), args.get_arg<double>(name(), "diff_window_size")),
                                          window_size_(args.get_arg<double>(name(), "window_size")),
                                          i_multi_producer<data_type>(new trivial_split_descriptor<data_type>())
    {
        initialize_trigger(args);
    }
    trigger_node(node_descriptor<mm_hit, mm_hit> *node_descriptor, const node_args &args) : window_state_(args.get_arg<double>(name(), "window_size"), args.get_arg<double>(name(), "diff_window_size")),
                                                                                            window_size_(args.get_arg<double>(name(), "window_size")),
                                                                                            i_multi_producer<data_type>(node_descriptor->split_descr)
    {
        initialize_trigger(args);
    }

    std::string name() override
    {
        return "trigger";
    }
    void process_all_buffered()
    {
        for (uint32_t i = 0; i < hit_buffer_.size(); i++)
        {
            this->writer_.write(std::move(hit_buffer_[i]));
        }
        hit_buffer_.clear();
    }
    void forget_old_hits(double current_time)
    {
        uint32_t buffer_index = 0;
        while (hit_buffer_.size() > 0 &&
               hit_buffer_.front().toa() + window_size_ < current_time)
        {
            hit_buffer_.pop_front();
            ++discarded_hit_count_;
        }
    }

    void process_window(window_state &state)
    {

        forget_old_hits(state.start_time());
        auto feat_vector = state.to_feature_vector();
        if (trigger_->trigger(feat_vector) && !triggered_) // we just found interesting hit
        {
            process_all_buffered();
            triggered_ = true;
        }
        if (trigger_->untrigger(feat_vector)) // we written out all high energy clusters for now,
            triggered_ = false;
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
                process_window(window_state_);

                window_state_.move_window();
                for (uint32_t i = 0; i < empty_count; i++)
                {
                    window_state_.to_feature_vector();
                    window_state_.move_window();
                }
            }
            if (triggered_)
                this->writer_.write(std::move(hit));
            else
                hit_buffer_.push_back(hit);
            ++processed;

            this->reader_.read(hit);
        }
        this->writer_.close();

        std::cout << "Discarded hit portion: " << discarded_hit_count_ / (double)processed << std::endl;
        // std::cout << "TRIGGER COMPUTER ENDED ----------------" << std::endl;
    }

    virtual ~trigger_node() = default;
};
