
#pragma once
#include <iomanip>
#include <iostream>
#include <numeric>
template <typename hit_type>
class default_window_feature_vector
{
    static constexpr uint16_t ENERGY_DISTR_BIN_COUNT_ = 17;
    static constexpr std::array<double, ENERGY_DISTR_BIN_COUNT_ - 1> ENERGY_DISTR_UPP_THL_ = {
        5., 10., 20., 30., 40., 50., 60., 70., 80., 90., 100., 150., 200., 400., 600., 1000.,
    };
    static constexpr double CLUSTERING_DT_ = 200.;
    double total_energy_ = 0.;
    double max_e_ = 0.;
    int64_t x_sum_ = 0;
    int64_t y_sum_ = 0;
    int64_t x2_sum_ = 0;
    int64_t y2_sum_ = 0;
    int64_t cluster_count_lower_bound_ = 0;
    double last_hit_toa_ = 0;
    double start_time_;
    std::array<double, ENERGY_DISTR_BIN_COUNT_> energy_distribution_{};
    public:
    template <typename data_type, typename stream_type> friend stream_type & operator << (stream_type & fstream,
     const default_window_feature_vector<data_type> &  window_feature_vect);
    template <typename data_type, typename stream_type> friend stream_type & operator >> (stream_type & fstream, 
     default_window_feature_vector<data_type> &  window_feature_vect);
    int64_t hit_count = 0;
   
    default_window_feature_vector(double start_time = 0):
    start_time_(start_time)
    {
        
    }

    bool is_valid() const
    {
        return hit_count >= 0;
    }
    uint32_t size() const
    {
        return 0;
    }
    static default_window_feature_vector<hit_type> end_token()
    {
        auto end_fw =  default_window_feature_vector<hit_type>(0);
        end_fw.hit_count = -1;
        return end_fw;
    }
    std::string distr_to_string() const
    {
        std::vector<std::string> energy_distrib_str;
        energy_distrib_str.resize(ENERGY_DISTR_BIN_COUNT_);
        std::transform(energy_distribution_.begin(), energy_distribution_.end(), energy_distrib_str.begin(), [](double x) {return std::to_string(int(round(x)));});        
        std::string result = "[";
        for (uint i = 0; i < energy_distrib_str.size(); i++)
        {
            result += " " + energy_distrib_str[i];
        }
        result += " ]";
        return result;
        //return "[ " + std::accumulate(energy_distrib_str.begin(), energy_distrib_str.end(), std::string(","))  + " ]";
    }
    void set_distr_from_str(const std::string & str)
    {
        char bracket;
        std::stringstream str_stream(str);
        str_stream >> bracket;
        for (uint32_t i = 0; i < ENERGY_DISTR_BIN_COUNT_; i++)
        {
            double hit_count;
            str_stream >> hit_count;
            energy_distribution_[i] = hit_count;
        }
        
    }
    std::vector<std::string> str() const
    {
        return std::vector<std::string>{ 
            std::to_string(start_time_ / 1000000),  
            std::to_string(mean_x()),
            std::to_string(mean_y()),
            std::to_string(std_x()),
            std::to_string(std_y()),
            std::to_string(hit_count),
            std::to_string(total_energy_),
            std::to_string(max_e_),
            std::to_string(cluster_count_lower_bound_),
            distr_to_string()
            };
    }
    static std::vector<std::string> attribute_names()
    {
        return std::vector<std::string>{"start_toa[ms]", "mean_x[px]", "mean_y[px]", "std_x[px]", "std_y[px]", 
        "hit_count[]", "tot_e[keV]", "max_e[keV]", "low_cl_est[]", "e_distrib[[]]"};
    }
    std::variant<double, std::vector<double>> get_value(const std::string & feature_name) const
    {
        std::variant<double, std::vector<double>> result;
        if(feature_name == attribute_names()[0])
            result = start_time_;
        else if (feature_name == attribute_names()[1])
            result = mean_x();
        else if (feature_name == attribute_names()[2])
            result = mean_y();
        else if (feature_name == attribute_names()[3])
            result = std_x();
        else if (feature_name == attribute_names()[4])
            result = std_y();
        else if (feature_name == attribute_names()[5])
            result = hit_count;
        else if (feature_name == attribute_names()[6])
            result = total_energy_;
        else if (feature_name == attribute_names()[7])
            result = max_e_;
        else if (feature_name == attribute_names()[8])
            result = cluster_count_lower_bound_;
        else if (feature_name == attribute_names()[9])
        //TODO
            result  = std::vector<double>(energy_distribution_.begin(),energy_distribution_.end()); 
        
        return result;
        
        
        
            
            
    }
    void update(const hit_type & hit)
    {
        
        if(hit.toa() - last_hit_toa_ > CLUSTERING_DT_)
        {
           ++cluster_count_lower_bound_; 
        }
        last_hit_toa_ = hit.toa();
        ++hit_count;
        total_energy_ += hit.e();
        if(hit.e() > max_e_)
            max_e_ = hit.e();
        x_sum_ += hit.x();
        y_sum_ += hit.y();
        x2_sum_ += hit.x() * hit.x();
        y2_sum_ += hit.y() * hit.y();

        for(uint32_t i = 0; i < ENERGY_DISTR_BIN_COUNT_ - 1; ++i)
        {
            if(hit.e() < ENERGY_DISTR_UPP_THL_[i])
            {
                energy_distribution_[i] += 1.;
                return;
            }
        }
        energy_distribution_[ENERGY_DISTR_BIN_COUNT_ - 1] += 1.;
    }
    double std_x() const
    {
        if(hit_count < 2)
            return std::nan("");
        double mean = mean_x();
        return std::sqrt((static_cast<double>(x2_sum_)/hit_count) - (mean)*(mean));
    }
    double std_y() const
    {
        if(hit_count < 2)
            return std::nan("");
        double mean = mean_y();
        return std::sqrt((static_cast<double>(y2_sum_)/hit_count) - (mean)*(mean));
    }
    double mean_x() const
    {
        if(hit_count == 0)
            return std::nan("");
        return x_sum_/(double)hit_count;
    }
    double mean_y() const
    {
        if(hit_count == 0)
            return std::nan("");
        return y_sum_/(double)hit_count;
    }
};
template < typename data_type, typename stream_type> 
stream_type & operator << (stream_type & fstream, const default_window_feature_vector<data_type> &  window_feature_vect)
{
    const char delim = ' ';
    (fstream << std::fixed << std::setprecision(6) <<
    window_feature_vect.start_time_ << delim << std::fixed << std::setprecision(6) <<
    window_feature_vect.cluster_count_lower_bound_ << delim <<
    window_feature_vect.hit_count << delim << 
    window_feature_vect.max_e_ << delim << 
    window_feature_vect.total_energy_ << delim <<
    window_feature_vect.mean_x() << delim <<
    window_feature_vect.mean_x() << delim <<
    window_feature_vect.std_x() << delim <<
    window_feature_vect.std_y() << delim <<
    window_feature_vect.distr_to_string() <<
    std::endl);
    return fstream;
}

template < typename data_type, typename stream_type>
stream_type & operator >> (stream_type & fstream, default_window_feature_vector<data_type> &  window_feature_vect)
{
    double mean_x, mean_y, std_x, std_y;   
    (fstream 
    >> window_feature_vect.start_time_
    >> window_feature_vect.cluster_count_lower_bound_
    >> window_feature_vect.hit_count
    >> window_feature_vect.max_e_
    >> window_feature_vect.total_energy_
    >> mean_x
    >> mean_y 
    >> std_x
    >> std_y);
    auto hit_count = window_feature_vect.hit_count;
    window_feature_vect.x_sum_ = hit_count > 0 ? mean_x * hit_count : 0;
    window_feature_vect.y_sum_ = hit_count > 0 ? mean_y * hit_count : 0;
    window_feature_vect.x2_sum_ = hit_count >= 2 ? (std_x * std_x + (mean_x * mean_x)) * hit_count : 0;
    window_feature_vect.y2_sum_ = hit_count >= 2 ? (std_y * std_y + (mean_y * mean_y)) * hit_count : 0;
    
    
    std::string e_str_distr = fstream.readLine().toStdString();
    std::cout << e_str_distr << " is the distribution str" << std::endl; 
    window_feature_vect.set_distr_from_str(e_str_distr);
    return fstream;
}

