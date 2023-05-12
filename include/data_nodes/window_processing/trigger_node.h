#include "../../data_flow/dataflow_package.h"
#include "window_state.h"
#include "default_window_feature_vector.h"

#include "triggers.h"
template <typename data_type, typename window_state = default_window_state<data_type>>
class trigger_node : public i_simple_consumer<data_type>, public i_simple_producer<data_type>
{
    //TODO EVERYTHING
    window_state window_state_;
    using trigger_type = abstract_window_trigger<data_type, default_window_feature_vector<data_type>>;
    std::unique_ptr<trigger_type> trigger_;
    bool triggered_ = false;
    std::deque<data_type> hit_buffer_;
    double window_size_;
    public:
    trigger_node(const node_args & args) :
    window_state_(args.get_arg<double>(name(), "window_size"), args.get_arg<double>(name(), "diff_window_size")),
    window_size_(args.get_arg<double>(name(), "window_size"))
    {
        const std::string INTERVAL_SUFFIX = ".ift";
        const std::string NN_SUFFIX = ".nnt";
        std::string trigger_file = args.get_arg<std::string>(name(), "trigger_file");
        if (ends_with(trigger_file, INTERVAL_SUFFIX))
            trigger_ = std::move(std::make_unique<interval_trigger<data_type>>(args.at(name())));
        if (ends_with(trigger_file, NN_SUFFIX))
            trigger_ = std::move(std::make_unique<onnx_trigger<data_type>>(args.at(name())));
        
    }
    std::string name() override
    {
        return "trigger";
    }
    void process_all_buffered()
    {
        for(uint32_t i = 0; i < hit_buffer_.size(); i++)
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
        }
    }

    void process_window(window_state & state) 
    {
        
        forget_old_hits(state.start_time());
        auto feat_vector = state.to_feature_vector();
        if(trigger_->trigger(feat_vector) && !triggered_ ) //we just found interesting hit
            {                                                      
                process_all_buffered();
                triggered_ = true;                           
            }  
        if(trigger_->untrigger(feat_vector))                          //we written out all high energy clusters for now, 
            triggered_ = false;
    }

    virtual void start() override
    {
        data_type hit;
        uint64_t processed = 0;
        this->reader_.read(hit);
        while(hit.is_valid())
        {
            if(window_state_.can_add(hit))
            {
                window_state_.update(hit);
                
            }
            else
            {
                uint32_t empty_count = window_state_.get_empty_count(hit);
                process_window(window_state_);
                
                window_state_.move_window();
                for(uint32_t i = 0; i < empty_count; i++)
                {
                    //indow_state_.to_feature_vector();    
                    window_state_.move_window();
                }


            }
            if(triggered_)
                    this->writer_.write(std::move(hit));
            else
                hit_buffer_.push_back(hit);
            ++processed; 
            
            this->reader_.read(hit);

        }
        this->writer_.write(data_type::end_token());
        this->writer_.flush();
        //std::cout << "TRIGGER COMPUTER ENDED ----------------" << std::endl;
    }

    virtual ~trigger_node() = default;
};
