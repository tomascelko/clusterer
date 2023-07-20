#pragma once
#include "default_window_feature_vector.h"
#include <deque>
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
    double window_time_; // 50ms
    double start_time_ = 0;
    double differential_window_;
    uint64_t hit_count_ = 0;
    default_window_feature_vector<hit_type> feature_vector_;
    std::deque<default_window_feature_vector<hit_type>> previous_vectors_;

public:
    uint64_t hit_count() const
    {
        return hit_count_;
    }
    double window_time() const
    {
        return window_time_;
    }

    default_window_state(double window_time, double differential_window = 0.) : window_time_(window_time),
                                                                                differential_window_(differential_window)
    {
        if (differential_window >= window_time)
        {
            auto feature_vect_copy = feature_vector_;
            feature_vect_copy.close();
            for (size_t i = 0; double(i) < differential_window_ / window_time_; ++i)
            {
                previous_vectors_.push_back(feature_vect_copy);
                // TODO copy feature vector and close it
            }
        }
    }
    double mean_frequency() const
    {
        return hit_count_ * 1000000000 / window_time_; // multiply by milion to convert from nanoseconds to seconds
    }
    double start_time() const
    {
        return start_time_;
    }
    bool can_add(const hit_type &hit) const
    {
        return hit.toa() <= start_time_ + window_time_;
    }
    uint64_t get_empty_count(const hit_type &hit)
    {
        double overtime = hit.toa() - (start_time_ + window_time_);
        return static_cast<uint64_t>(overtime) / static_cast<uint64_t>(std::round(window_time_));
    }
    void update(const hit_type &hit)
    {
        last_hit_toa_ = hit.toa();
        ++hit_count_;
        feature_vector_.update(hit);
    }
    double last_hit_toa() const
    {
        return last_hit_toa_;
    }
    void move_window()
    {
        if (previous_vectors_.size() > 0)
        {
            previous_vectors_.pop_front();
            previous_vectors_.push_back(feature_vector_);
        }
        start_time_ += window_time_;
        hit_count_ = 0;
        feature_vector_ = default_window_feature_vector<hit_type>(start_time_);
    }
    bool is_valid()
    {
        return feature_vector_.is_valid();
    }
    default_window_feature_vector<hit_type> to_feature_vector()
    {
        feature_vector_.close();
        if (previous_vectors_.size() > 0)
            return feature_vector_.diff_with_median(previous_vectors_);
        return feature_vector_;
    }
};

template <typename hit_type>
class window_frequency_state
{
    const double MIN_WINDOW_TIME_ = 50000000; // 50ms
    double first_hit_toa_ = 0;
    double last_hit_toa_ = 0;
    uint64_t hit_count_ = 0;

public:
    bool is_end() const
    {
        return last_hit_toa_ - first_hit_toa_ > MIN_WINDOW_TIME_;
    }
    void update(const hit_type &hit)
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
        5.,
        10.,
        20.,
        30.,
        50.,
        100.,
        200.,
        400.,
        600.,
        1000.,
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
    void update(const hit_type &hit)
    {
        for (uint16_t i = 0; i < energy_distr_.size(); ++i)
        {
            if (hit.e() < ENERGY_DISTR_UPP_THL_[i])
            {
                ++energy_distr_[i];
                break;
            }
        }
        if (hit.e() > ENERGY_DISTR_UPP_THL_.back())
        {
            ++energy_distr_[energy_distr_.size() - 1];
        }
    }
    void reset()
    {
        first_hit_toa_ = last_hit_toa_;
        energy_distr_ = std::array<double, ENERGY_DISTR_BIN_COUNT_>();
    }
    energy_dist_state() : energy_distr_(std::array<double, ENERGY_DISTR_BIN_COUNT_>())
    {
    }
};
