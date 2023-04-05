#pragma once
#include "../analysis/default_window_feature_vector.h"
template <typename hit_type>
class default_window_state 
{
    /*computes :
        hit frequency
        energy distribution
        max energy


        variance in x and in y coordinate

    */
   double last_hit_toa_ = 0;
   double first_hit_toa_ = 0;
    double window_time_; //50ms
    double start_time_ = 0;
    default_window_feature_vector<hit_type> feature_vector_;

    public:
    default_window_state(double window_time) :
    window_time_(window_time)
    {

    }
    bool can_add(const hit_type & hit) const
    {
        return hit.toa() <= start_time_ + window_time_;
    }
    uint64_t get_empty_count(const hit_type & hit)
    {
        double overtime = hit.toa() - (start_time_ + window_time_);
        return static_cast<uint64_t>(overtime) / static_cast<uint64_t>(std::round(window_time_));
        
    }
    void update(const hit_type & hit)
    {
        last_hit_toa_ = hit.toa();
        feature_vector_.update(hit);

    }
    void move_window()
    {
        start_time_ += window_time_;
        feature_vector_ = default_window_feature_vector<hit_type>(start_time_);
    }
    bool is_valid()
    {
        return feature_vector_.is_valid();
    }
    default_window_feature_vector<hit_type> to_feature_vector()
    {
        return feature_vector_;
    }
};

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

