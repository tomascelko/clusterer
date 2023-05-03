#include "../../data_flow/dataflow_package.h"
#include "window_state.h"
template <typename data_type, typename trigger_type, typename window_state = default_window_state<data_type>>
class trigger_node : public i_simple_consumer<data_type>, public i_simple_producer<data_type>
{
    //TODO EVERYTHING
    window_state window_state_;
    trigger_type trigger_;
    bool triggered_ = false;
    std::deque<data_type> hit_buffer_;
    double window_size_;
    public:
    trigger_node(const node_args & args) :
    window_state_(args.get_double_arg(name(), "window_size")),
    window_size_(args.get_double_arg(name(), "window_size"))
    {
        trigger_()
    }
    std::string name() override
    {
        return "trigger";
    }
    void process_all_buffered()
    {
        for(uint32_t i = 0; i < hit_buffer_; i++)
        {
            writer->write(hit_buffer_[i])
        }
    }
    void process_window(window_state & state) 
    {
        
        forget_old_hits(state.start_time());
        auto feat_vector = state.to_feature_vector();
        if(!triggered_ && trigger_.trigger(feat_vector)) //we just found interesting hit
            {                                                      
                process_all_buffered();
                triggered_ = true;                           
            }  
            if(trigger_.untrigger(feat_vector))                          //we written out all high energy clusters for now, 
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
                if(triggered_)
                    writer_->write(hit);
                else
                    hit_buffer_.push_back(hit);
                window_state_.move_window();
                for(uint32_t i = 0; i < empty_count; i++)
                {
                    //indow_state_.to_feature_vector();    
                    window_state_.move_window();
                }


            }
            ++processed; 
            
            this->reader_.read(hit);

        }
        this->writer_.write(data_type::end_token());
        this->writer_.flush();
        std::cout << "TRIGGER COMPUTER ENDED ----------------" << std::endl;
    }

    virtual ~window_feature_computer() = default;
};

template <typename hit_type, typename state_type>
class abstract_trigger
{
    protected:
    double last_triggered_ = 0;
    double untrigger_time_;  //in ns, set to 400 miliseconds (2 windows)
    public:
    abstract_trigger(double untrigger_time = 200000000) :
    untrigger_time_(untrigger_time)
    {}
    virtual bool trigger(const state_type & new_state)
    {
        bool triggered = should_trigger(new_state);

        if(triggered)
        {
            last_triggered_ = new_state.last_hit_toa();
        }
        return triggered;

    }
    virtual bool should_trigger(const state_type & new_state) = 0;
    virtual bool untrigger(const state_type & new_state)
    {
        return new_state.last_hit_toa() - last_triggered_ > untrigger_time_;
    }
};


template <typename hit_type>
class interval_trigger : public abstract_trigger<hit_type, default_window_feature_vector<hit_type>>
{
    struct interval
    {
        double lower, double upper;
        interval(double lower, double upper):
        lower(lower), upper(upper){}
    }
    bool verbose = true;
    uint32_t vector_count_ = 1;
    std::vector<std::map<std::string, interval>> intervals_;
    public:
    using state_type = default_window_feature_vector;
    interval_trigger(const std::map<std::string, std::string> & args) :
    abstract_trigger<hit_type>(std::stod(args["untrigger_time"]))
    {
        std::string interval_file = args["trigger_file"];

    }
    virtual bool should_trigger(const default_window_feature_vector<hit_type> & feat_vector) override
    {
        for(auto && interval_feature_map : intervals_)
        {
            for(auto && feature_name : feat_vector.attribute_names())
            {
            
                double feature_value = feat_vector.get_scalar(feature_name);
                feature
                
            }
        }
    }
};

