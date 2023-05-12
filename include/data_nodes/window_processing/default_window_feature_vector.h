
#pragma once
#include <iomanip>
#include <iostream>
#include <numeric>
#include <variant>
#include <map>
#include <deque>
#include <fstream>
#include <algorithm>
template <typename hit_type>
class default_window_feature_vector
{
    static constexpr uint16_t ENERGY_DISTR_BIN_COUNT_ = 17;
    static constexpr std::array<double, ENERGY_DISTR_BIN_COUNT_ - 1> ENERGY_DISTR_UPP_THL_ = {
        5.,
        10.,
        20.,
        30.,
        40.,
        50.,
        60.,
        70.,
        80.,
        90.,
        100.,
        150.,
        200.,
        400.,
        600.,
        1000.,
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
    std::vector<double> energy_distribution_ = std::vector<double>(ENERGY_DISTR_BIN_COUNT_);

public:
    
    std::map<std::string, std::vector<double>> vector_features;
    std::map<std::string, double> scalar_features;
    template <typename data_type, typename stream_type>
    friend stream_type &operator<<(stream_type &fstream,
                                   const default_window_feature_vector<data_type> &window_feature_vect);
    template <typename data_type, typename stream_type>
    friend stream_type &operator>>(stream_type &fstream,
                                   default_window_feature_vector<data_type> &window_feature_vect);
    int64_t hit_count = 0;
    double start_time()
    {
        return start_time_;
    }
    bool closed = false;
    void close()
    {

        scalar_features[attribute_names()[0]] = start_time_;
        scalar_features[attribute_names()[1]] = mean_x();
        scalar_features[attribute_names()[2]] = mean_y();
        scalar_features[attribute_names()[3]] = std_x();
        scalar_features[attribute_names()[4]] = std_y();
        scalar_features[attribute_names()[5]] = hit_count;
        scalar_features[attribute_names()[6]] = total_energy_;
        scalar_features[attribute_names()[7]] = max_e_;
        scalar_features[attribute_names()[8]] = hit_count > 0 ? hit_count / cluster_count_lower_bound_ : 0;
        vector_features[attribute_names()[9]] = energy_distribution_;
        closed = true;
        
    }
    default_window_feature_vector(double start_time = 0) : start_time_(start_time)
    {
    }
    std::vector<double> to_vector() const
    {
        std::vector<double> result;
        for(auto && attribute_name : attribute_names())
        {
            if(scalar_features.find(attribute_name) != scalar_features.end())
            {
                //TODO remove start toa feature
                result.push_back(scalar_features.at(attribute_name));
            }
            else
            {
                const std::vector<double> & vector_attribute = vector_features.at(attribute_name);

                result.insert(result.end(), vector_attribute.cbegin(), vector_attribute.cend());  
            }     
        }
        return result;
    }
    default_window_feature_vector<hit_type> diff_with_median(const std::deque<default_window_feature_vector<hit_type>> & previous_vectors)
    {
        using vector_type = std::vector<double>;
        using scalar_type = double;
        const std::string VECT_STR = "[[";
        size_t median_index = previous_vectors.size() / 2;
        default_window_feature_vector<hit_type> diff_vector;
        for (auto & attribute_name : attribute_names())
        {

            std::vector<vector_type> attribute_vectors;
            std::vector<scalar_type> attribute_scalars;
            for (auto & previous_vector : previous_vectors)
            {
                //auto attribute_value = previous_vector.get_value(attribute_name);
                if (is_vector(attribute_name))
                {
                    attribute_vectors.push_back(previous_vector.get_vector(attribute_name));
                }
                else
                {
                    attribute_scalars.push_back(previous_vector.get_scalar(attribute_name));
                }
            }
            
            if (is_vector(attribute_name))
            {
                vector_type current_value = std::get<vector_type>(get_value(attribute_name));
                diff_vector.set_vector(attribute_name, current_value);
                //TODO
            }
            else
            {
                double current_value = std::get<scalar_type>(get_value(attribute_name));
                if(attribute_name == attribute_names()[0])
                {
                    diff_vector.set_scalar(attribute_name, current_value);
                }
                else
                {
                    std::nth_element(attribute_scalars.begin(), attribute_scalars.begin() + median_index, attribute_scalars.end());
                    diff_vector.set_scalar(attribute_name, current_value - attribute_scalars[median_index]);
                }
            }
        }
        diff_vector.last_hit_toa_ = last_hit_toa_;
        diff_vector.hit_count = hit_count;
        return diff_vector;
    }
    bool is_valid() const
    {
        return hit_count >= 0;
    }
    uint32_t size() const
    {
        return 0;
    }
    double last_hit_toa() const
    {
        return last_hit_toa_;
    }
    static default_window_feature_vector<hit_type> end_token()
    {
        auto end_fw = default_window_feature_vector<hit_type>(0);
        end_fw.hit_count = -1;
        return end_fw;
    }
    void write_vector(std::ofstream & stream, const std::vector<double> & vector_attribute) const
    {
        stream << "[";
        for (uint i = 0; i < vector_attribute.size(); ++i)
        {
            stream << std::fixed << std::setprecision(2) << " " << vector_attribute[i];
        }
        stream << " ]";
        
        // return "[ " + std::accumulate(energy_distrib_str.begin(), energy_distrib_str.end(), std::string(","))  + " ]";
    }
    template<typename stream_type>
    void read_vector(stream_type & stream, std::vector<double> & result)
    {
        char symbol;
        stream.get(symbol);
        stream.get(symbol);
        stream.get(symbol);

        std::string vector_str = "";
        while (symbol != ']')
        {
            vector_str += symbol;
            stream.get(symbol);
        }
        std::cout << vector_str << std::endl;
        std::stringstream vector_ss(vector_str);
        while (!vector_ss.eof())
        {
            double value;
            vector_ss >> value;
            result.push_back(value);
        }
    }
    /*std::vector<std::string> str() const
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
            distr_to_string()};
    }*/
    static std::vector<std::string> attribute_names()
    {
        return std::vector<std::string>{"start_toa[ms]", "mean_x[px]", "mean_y[px]", "std_x[px]", "std_y[px]",
                                        "hit_count[]", "tot_e[keV]", "max_e[keV]", "cl_size_upp_bound[]", "e_distrib[[]]"};
    }
    void set_scalar(const std::string &key, double value)
    {
        scalar_features[key] = value;
    }
    void set_vector(const std::string &key, const std::vector<double> &value)
    {
        vector_features[key] = value;
    }
    const double & get_scalar(const std::string & key) const
    {
        return scalar_features.at(key);
    }
    const std::vector<double> & get_vector(const std::string & key) const
    {
        return vector_features.at(key);
    }
    bool is_vector(const std::string &feature) const
    {
        return feature.find("[[") != std::string::npos;
    }
    

    std::variant<double, std::vector<double>> get_value(const std::string & feature_name) const
    {
        std::variant<double, std::vector<double>> result;
        if (is_vector(feature_name))
            result = vector_features.at(feature_name);
        else 
            result = scalar_features.at(feature_name);
        return result;
    }
    void update(const hit_type &hit)
    {

        if (hit.toa() - last_hit_toa_ > CLUSTERING_DT_)
        {
            ++cluster_count_lower_bound_;
        }
        last_hit_toa_ = hit.toa();
        ++hit_count;
        total_energy_ += hit.e();
        if (hit.e() > max_e_)
            max_e_ = hit.e();
        x_sum_ += hit.x();
        y_sum_ += hit.y();
        x2_sum_ += hit.x() * hit.x();
        y2_sum_ += hit.y() * hit.y();

        for (uint32_t i = 0; i < ENERGY_DISTR_BIN_COUNT_ - 1; ++i)
        {
            if (hit.e() < ENERGY_DISTR_UPP_THL_[i])
            {
                energy_distribution_[i] += 1.;
                return;
            }
        }
        energy_distribution_[ENERGY_DISTR_BIN_COUNT_ - 1] += 1.;
    }
    double std_x() const
    {
        if (hit_count < 2)
            return std::nan("");
        double mean = mean_x();
        if (static_cast<double>(x2_sum_) / hit_count < (mean) * (mean))
            return 0;
        return std::sqrt((static_cast<double>(x2_sum_) / hit_count) - (mean) * (mean));
    }
    double std_y() const
    {
        if (hit_count < 2)
            return std::nan("");
        double mean = mean_y();
        if (static_cast<double>(y2_sum_) / hit_count < (mean) * (mean))
            return 0;
        return std::sqrt((static_cast<double>(y2_sum_) / hit_count) - (mean) * (mean));
    }
    double mean_x() const
    {
        if (hit_count == 0)
            return std::nan("");
        return x_sum_ / (double)hit_count;
    }
    double mean_y() const
    {
        if (hit_count == 0)
            return std::nan("");
        return y_sum_ / (double)hit_count;
    }
};
template <typename data_type, typename stream_type>
stream_type &operator<<(stream_type &fstream, const default_window_feature_vector<data_type> &window_feature_vect)
{

    const char delim = ' ';
    for (auto feature_name : window_feature_vect.attribute_names())
    {
        if (window_feature_vect.is_vector(feature_name))
        {
            std::vector<double> vector_feature;
            window_feature_vect.write_vector(fstream, window_feature_vect.vector_features.at(feature_name));
            
        }
        else
        {
            fstream << std::fixed << std::setprecision(6) << window_feature_vect.scalar_features.at(feature_name);     
        }
        fstream << delim;    
    }
    fstream << std::endl;
    return fstream;
}

template <typename data_type, typename stream_type>
stream_type &operator>>(stream_type &fstream, default_window_feature_vector<data_type> &window_feature_vect)
{
    double mean_x, mean_y, std_x, std_y;
    for (auto feature_name : window_feature_vect.attribute_names())
    {
        if (window_feature_vect.is_vector(feature_name))
        {
            std::vector<double> vector_feature;
            window_feature_vect.read_vector(fstream, vector_feature);
            window_feature_vect.vector_features[feature_name] = vector_feature;
        }
        else
        {
            std::string scalar_feature_str;
            double scalar_feature;
            fstream >> scalar_feature_str;
            std::cout << scalar_feature_str << std::endl;
            if(scalar_feature_str == "nan")
                scalar_feature = std::numeric_limits<double>::quiet_NaN();
            else
            {
                std::replace(scalar_feature_str.begin(), scalar_feature_str.end() ,'.', ',');
                scalar_feature = std::stod(scalar_feature_str);
            }
            window_feature_vect.scalar_features[feature_name] = scalar_feature;     
        }
    }
    window_feature_vect.closed = true;

    /*window_feature_vect.x_sum_ = hit_count > 0 ? mean_x * hit_count : 0;
    window_feature_vect.y_sum_ = hit_count > 0 ? mean_y * hit_count : 0;
    window_feature_vect.x2_sum_ = hit_count >= 2 ? (std_x * std_x + (mean_x * mean_x)) * hit_count : 0;
    window_feature_vect.y2_sum_ = hit_count >= 2 ? (std_y * std_y + (mean_y * mean_y)) * hit_count : 0;
    
    std::string e_str_distr = fstream.readLine().toStdString();
    std::cout << e_str_distr << " is the distribution str" << std::endl; 
    window_feature_vect.set_distr_from_str(e_str_distr);
    */
    return fstream;
};
