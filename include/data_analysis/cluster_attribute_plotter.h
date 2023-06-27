#include "../../matplotlib-cpp/matplotlibcpp.h"
#pragma once
namespace plt = matplotlibcpp;
template <typename x_data_type, typename y_data_type>
class histo_2D_plotter
{
    const uint32_t X_BUCKETS = 10;
    const uint32_t Y_BUCKETS = 10;
    public:
    using histo_2D_container_type = std::vector<float>;
    histo_2D_container_type histo_data_;
    template<typename number_type>
    uint32_t find_bucket_index(number_type value, number_type mini, number_type maxi, uint32_t bucket_count)
    {
        double relative_position = (value - mini)/static_cast<double>(maxi-mini);
        uint32_t min_cropped = std::max(static_cast<int32_t>(std::floor(relative_position * bucket_count)),0);
        return std::min(bucket_count - 1, min_cropped);
    }
    void show_histogram(const std::vector<x_data_type>& x_data, const std::vector<y_data_type>& y_data)
    {
        histo_data_ = histo_2D_container_type(X_BUCKETS * Y_BUCKETS, 0.);
        assert(x_data.size() == y_data.size());
        auto max_x = *std::max_element(x_data.begin(), x_data.end());
        auto min_x = *std::min_element(x_data.begin(), x_data.end());
        auto max_y = *std::max_element(y_data.begin(), y_data.end());
        auto min_y = *std::min_element(y_data.begin(), y_data.end());
        for (uint32_t i = 0; i < x_data.size(); ++i)
        {
            uint32_t hist_x = find_bucket_index(x_data[i], min_x, max_x, X_BUCKETS);
            uint32_t hist_y = find_bucket_index(y_data[i], min_y, max_y, Y_BUCKETS);
            std::cout << hist_x << " " << hist_y << std::endl;
            std::cout << hist_y + (X_BUCKETS - hist_x - 1) * Y_BUCKETS << std::endl;
            histo_data_[hist_y +  hist_x * Y_BUCKETS] += 1;
        }
        auto max_frequency = *std::max_element(histo_data_.begin(), histo_data_.end());
        std::cout << "max frewq " << max_frequency << std::endl; 
        for (uint32_t i = 0; i < histo_data_.size(); ++i)
        {
            histo_data_[i] /= static_cast<double>(max_frequency);
            std::cout << histo_data_[i] << " " << std::endl;
        }
        plt::imshow(&histo_data_[0], X_BUCKETS, Y_BUCKETS, 1);
        //plt::xlim(min_x - 0.5, max_x - 0.5);
        //plt::ylim(min_y - 0.5, max_y - 0.5);
        plt::show();
    }
};