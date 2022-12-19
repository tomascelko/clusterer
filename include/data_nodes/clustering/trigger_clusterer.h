#include "clusterer.h"
#include "../../devices/current_device.h"
#include <optional>
#pragma once


template <typename hit_type>
class window_frequency_state
{
    const double MIN_WINDOW_TIME_ = 50000000; //50ms
    double first_hit_toa_ = 0;
    double last_hit_toa_ = 0;
    uint64_t hit_count_ = 0;
    public:
    bool is_end() const
    {
        return last_hit_toa_ - first_hit_toa_ > MIN_WINDOW_TIME_;
    }
    void update(const hit_type & hit)
    {
        ++hit_count_;
        last_hit_toa_ = hit.toa();
    }
    void reset()
    {
        first_hit_toa_ = last_hit_toa_;
        hit_count_ = 1; 
    }
    uint64_t hit_count() const
    {
        return hit_count_;
    }
    double window_width_actual() const
    {
        return last_hit_toa_ - first_hit_toa_;
    }
    double last_hit_toa() const
    {
        return last_hit_toa_;
    }


};
template <typename hit_type>
class energy_dist_state
{
    static constexpr uint16_t ENERGY_DISTR_BIN_COUNT_ = 11; 
    static constexpr std::array<double, ENERGY_DISTR_BIN_COUNT_ - 1> ENERGY_DISTR_UPP_THL_ = {
        5., 10., 20., 30., 50., 100., 200., 400., 600., 1000.,
    };
    const double MIN_WINDOW_TIME_ = 50000000;
    double first_hit_toa_ = 0;
    double last_hit_toa_ = 0;
    std::array<double, ENERGY_DISTR_BIN_COUNT_> energy_distr_;
    public:
    bool is_end()
    {
        return last_hit_toa_ - first_hit_toa_ > MIN_WINDOW_TIME_;
    }
    void update(const hit_type & hit)
    {
        for(uint16_t i = 0; i < energy_distr_.size() ; ++i)
        {
            if(hit.e() < ENERGY_DISTR_UPP_THL_[i])
            {
                ++energy_distr_[i];
                break;
            }
        }
        if(hit.e() > ENERGY_DISTR_UPP_THL_.back())
        {
            ++energy_distr_[energy_distr_.size() - 1];
        }

    }
    void reset()
    {
        first_hit_toa_ = last_hit_toa_;
        energy_distr_ = std::array<double, ENERGY_DISTR_BIN_COUNT_>();
    }
    energy_dist_state() :
    energy_distr_(std::array<double, ENERGY_DISTR_BIN_COUNT_>())
    {}
    

};



template <typename hit_type, typename clustering_alg_type, typename trigger_type>
class trigger_clusterer : public i_simple_consumer<hit_type>, 
                          public i_data_producer<cluster<hit_type>>,
                          public i_time_measurable
{
    protected:    
    
    measuring_clock* clock_;
    const uint32_t BUFFER_CHECK_INTERVAL = 2;
    const uint32_t WRITE_INTERVAL = 2 << 6;
    const double BUFFER_FORGET_DT = 50000000;
    
    bool finished_ = false;
    trigger_type trigger_;
    typename trigger_type::state_type window_state_;
    protected:
    uint64_t processed_hit_count_;
    using buffer_type = std::deque<hit_type>;
    //const uint32_t expected_buffer_size = 2 << ;
    buffer_type hit_buffer_;
    //double toa_crit_interval_end_ = 0;
    enum class clusterer_state
    {
        processing,
        monitoring
    };
    clusterer_state current_state;
    std::unique_ptr<clustering_alg_type> clusterer_;
    
    
    
    void wrapped_process_hit(hit_type & hit) //used for benchmarking
    {
        if(processed_hit_count_ % WRITE_INTERVAL == 0) 
            clusterer_->write_old_clusters(hit.toa());
        window_state_.update(hit);
        bool end_of_window = window_state_.is_end();
        if(processed_hit_count_ % BUFFER_CHECK_INTERVAL == 0)
            forget_old_hits(hit.toa());
        
        if(current_state == clusterer_state::processing)
            clusterer_->process_hit(hit);
        else if(current_state == clusterer_state::monitoring)
            hit_buffer_.push_back(hit);

        if(end_of_window)
        {                                                      //and we were in forgetting phase
            if(current_state == clusterer_state::monitoring && trigger_.trigger(window_state_)) //we just found interesting hit
            {                                                      
                process_all_buffered();                           
            }  
            if(trigger_.untrigger(window_state_))                          //we written out all high energy clusters for now, 
                current_state = clusterer_state::monitoring;  //so we can start monitoring again     
            window_state_.reset();                      
        }
                                    //and process all hits retrospectively
    }
    void forget_old_hits(double current_toa) //removes old hits from buffer
    {
        while(hit_buffer_.size() > 0 && hit_buffer_.front().toa() <  current_toa - BUFFER_FORGET_DT)
        {
            hit_buffer_.pop_front();
        }
    }
    
    void process_all_buffered()
    {
        current_state = clusterer_state::processing;
        uint64_t buffer_size =  hit_buffer_.size();
        for(uint32_t i = 0; i < buffer_size; ++i)
        {
            clusterer_->process_hit(hit_buffer_[i]);
            if(i % WRITE_INTERVAL == 0) 
                clusterer_->write_old_clusters(hit_buffer_[i].toa());
             //we can remove the hit from buffer, 
        }                            //as it is now located inside of a unfinished clusters data structure
        hit_buffer_.clear();
 
        //TODO when to start monitoring again ? set toa_crit_interval_end_
        //TODO try buffered printer
    }
    void write_remaining_clusters()
    {
        clusterer_->write_remaining_clusters();
    }
    
    public:
    virtual void prepare_clock(measuring_clock* clock)
    {
        clock_ = clock;    
    }
    void connect_output(default_pipe<cluster<hit_type>> * pipe) override
    {
        this->clusterer_->connect_output(pipe);
    }
    std::string name() override
    {
        return "trigger_clusterer";
    }
    virtual void start() override
    {
        hit_type hit;
        this->reader_.read(hit);
        int num_hits = 0;
        clock_->start();
        while(hit.is_valid())
        {
            wrapped_process_hit(hit);
            processed_hit_count_++;
            this->reader_.read(hit);
        }
        std::cout << "++++++++++" << std::endl;
        write_remaining_clusters();
        clock_->stop_and_report("trigger clusterer");


        std::cout << "TRIGGER CLUSTERER ENDED -------------------" << std::endl;    
    }
    template <typename ... underlying_clusterer_args_type>
    trigger_clusterer(underlying_clusterer_args_type... underlying_clusterer_args ) :
    processed_hit_count_(0),
    clusterer_(std::move(std::make_unique<clustering_alg_type>(underlying_clusterer_args...)))
    {   

    }
    virtual ~trigger_clusterer() = default;
};

template <typename hit_type, typename state_type>
class abstract_trigger
{
    protected:
    double last_triggered_ = 0;
    const double UNTRIGGER_THRESHOLD_ = 100000000;  //in ns, set to 100 miliseconds (2 windows)
    std::optional<state_type> old_state_;
    public:
    virtual bool trigger(const state_type & new_state)
    {
        if (!old_state_.has_value())
        {
            old_state_.emplace(new_state);  //storing a copy of the current state
            return true;
        }
        bool triggered = unwrapped_trigger(new_state);
        old_state_.emplace(new_state);
        if(triggered)
        {
            last_triggered_ = new_state.last_hit_toa();
        }
        return triggered;

    }
    virtual bool unwrapped_trigger(const state_type & new_state) = 0;
    virtual bool untrigger(const state_type & new_state)
    {
        return new_state.last_hit_toa() - last_triggered_ > UNTRIGGER_THRESHOLD_;
    }
};

template <typename hit_type>
class frequency_diff_trigger : public abstract_trigger<hit_type, window_frequency_state<hit_type>>
{
    bool verbose = true;
    public:
    using state_type = window_frequency_state<hit_type>;
    virtual bool unwrapped_trigger(const state_type & new_state) override
    {
        double old_freq = this->old_state_->hit_count() * 1000 / this->old_state_->window_width_actual();
        double new_freq = new_state.hit_count() * 1000 / new_state.window_width_actual();
        bool triggered = ((old_freq > 0 && (new_freq / old_freq) > 2)
         || (new_freq > 0 && (old_freq / new_freq) > 2));
        if(triggered)
        {
            if (verbose)
            {
                std::cout << "OLD FREQ " << old_freq << " MHit/s" << std::endl;
                std::cout << "NEW_FREQ " << new_freq << " MHit/s" << std::endl;
            }
        }
        return triggered;      
    }
};
//TODO implement energy distr trigger but before that try to compile passing trigger type as an argument