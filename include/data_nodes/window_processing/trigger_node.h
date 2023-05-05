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
    window_state_(args.get_double_arg(name(), "window_size"), args.get_double_arg(name(), "diff_window_size")),
    window_size_(args.get_double_arg(name(), "window_size")),
    trigger_(args.at(name()))
    {
        
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
        if(trigger_.trigger(feat_vector) && !triggered_ ) //we just found interesting hit
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
        std::cout << "TRIGGER COMPUTER ENDED ----------------" << std::endl;
    }

    virtual ~trigger_node() = default;
};

template <typename hit_type, typename state_type>
class abstract_window_trigger
{
    protected:
    double last_triggered_ = 0;
    double untrigger_time_;  //in ns, set to 400 miliseconds (2 windows)
    public:
    abstract_window_trigger(double untrigger_time = 200000000) :
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
class interval_trigger : public abstract_window_trigger<hit_type, default_window_feature_vector<hit_type>>
{
    struct interval
    {
        double lower, upper;
        interval(double lower, double upper):
        lower(lower), upper(upper){}
    };
    bool verbose = true;
    uint32_t vector_count_ = 1;
    std::vector<std::map<std::string, interval>> intervals_;
    std::vector<std::string> attribute_names_;
    interval parse_interval_bounds(const std::string& interval_str)
    {
        const char INTERVAL_DELIM = ':';
        const std::string ANY_VALUE_STR = "*";
        auto int_delim_pos = interval_str.find(INTERVAL_DELIM);
        auto lower_bound_str = interval_str.substr(0, int_delim_pos);
        auto upper_bound_str = interval_str.substr(int_delim_pos + 1);
        double lower_bound, upper_bound;
        if (lower_bound_str == ANY_VALUE_STR)
            lower_bound = -std::numeric_limits<double>::infinity();
        else
            lower_bound = std::stod(lower_bound_str);
        if (upper_bound_str == ANY_VALUE_STR)
            upper_bound = std::numeric_limits<double>::infinity();
        else
            upper_bound = std::stod(upper_bound_str);
        return interval(lower_bound, upper_bound);
    }
    
    void load_intervals(const std::string & interval_file)
    {
        const char DELIM = ' ';
        std::ifstream interval_stream(interval_file);
        if (!interval_stream.is_open())
            throw std::invalid_argument("could not load the trigger file: " + interval_file);
        std::string stream_line;
        std::getline(interval_stream, stream_line);
        std::stringstream attribute_ss(stream_line);
        std::string attribute_name;
        while(std::getline(attribute_ss, attribute_name, DELIM))
        {
            attribute_names_.push_back(attribute_name);
        }
        std::stringstream interval_ss;
        
        std::string interval_str;
        while(std::getline(interval_stream, stream_line))
        {
            std::map<std::string, interval> interval_map;
            interval_ss = std::stringstream(stream_line);
            uint32_t interval_index = 0;
            while(std::getline(interval_ss, interval_str, DELIM))
            {
                auto trimmed_interval_str = interval_str.substr(1, interval_str.size() - 2);
                interval bound_interval = parse_interval_bounds(trimmed_interval_str);
                interval_map.insert(std::make_pair(attribute_names_[interval_index], bound_interval));
                ++interval_index;
            }
           intervals_.emplace_back(interval_map);
        }
    }
    public:
    using state_type = default_window_feature_vector<hit_type>;
    interval_trigger(const std::map<std::string, std::string> & args) :
    abstract_window_trigger<hit_type, default_window_feature_vector<hit_type>>(std::stod(args.at("trigger_time")))
    {
        std::string interval_file = args.at("trigger_file");
        load_intervals(interval_file);

    }
    virtual bool should_trigger(const default_window_feature_vector<hit_type> & feat_vector) override
    {
        for(auto && interval_feature_map : intervals_)
        {
            bool start_trigger = true;
            for(auto && feature_name : attribute_names_)
            {
            
                double feature_value = feat_vector.get_scalar(feature_name);
                interval & feat_interval = interval_feature_map.at(feature_name);
                if (std::isinf(-feat_interval.lower) && std::isinf(feat_interval.upper))
                    continue;
                if (feature_value < feat_interval.lower || feature_value > feat_interval.upper)
                {    
                    start_trigger = false;
                    break;
                }
            }
            if(start_trigger)
                return true;
        }
        return false;
    }
};

