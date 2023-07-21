
#pragma once
#include <iomanip>
#include <iostream>
#include <numeric>
#include <variant>
#include <map>
#include <deque>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
//a feature vector which contains a set of features aggregted for a timewindow
//it is also used to initiate clustering in trigger  architecture
template <typename hit_type>
class default_window_feature_vector
{
    static constexpr uint16_t ENERGY_DISTR_BIN_COUNT_ = 17;
    //thresholds for distributions of pixel energy in keV
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
    //dt for temporal clustering feature
    static constexpr double CLUSTERING_DT_ = 200.;
    double total_energy_ = 0.;
    double max_e_ = 0.;
    int64_t x_sum_ = 0;
    int64_t y_sum_ = 0;
    //squared sums for computation of coordinate variance
    int64_t x2_sum_ = 0;
    int64_t y2_sum_ = 0;
    int64_t cluster_count_lower_bound_ = 0;
    double last_hit_toa_ = 0;
    double start_time_;
    std::vector<double> energy_distribution_ = std::vector<double>(ENERGY_DISTR_BIN_COUNT_);

public:
    static constexpr uint64_t avg_size()
    {
        return (attribute_names().size() - 1 + ENERGY_DISTR_BIN_COUNT_) * sizeof(double);
    }
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
    //close the window = no additional hits will be added to this instance
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
        scalar_features[attribute_names()[9]] = hit_count > 0 ? total_energy_ / cluster_count_lower_bound_ : 0;
        std::vector<double> energy_distr_norm;
        vector_features[attribute_names()[10]] = normalized(energy_distribution_);
        closed = true;
    }
    default_window_feature_vector(double start_time = 0) : start_time_(start_time)
    {
    }
    //normalize the vector so the sum of all elements is equal to 1
    std::vector<double> normalized(const std::vector<double> &vect)
    {
        std::vector<double> result;
        const double EPSILON = 0.00001;
        double sum = 0;
        for (const auto value : vect)
        {
            sum += std::abs(value);
        }
        if (sum < EPSILON)
            return vect;
        for (const auto value : vect)
        {
            result.push_back(value / (double)sum);
        }
        return result;
    }
    //convert this object to feature vector of real numbers
    std::vector<double> to_vector(bool replace_nan = false) const
    {
        std::vector<double> result;
        for (auto &&attribute_name : attribute_names())
        {
            if (scalar_features.find(attribute_name) != scalar_features.end())
            {
                
                result.push_back(scalar_features.at(attribute_name));
            }
            else if (vector_features.find(attribute_name) != vector_features.end())
            {
                const std::vector<double> &vector_attribute = vector_features.at(attribute_name);

                result.insert(result.end(), vector_attribute.cbegin(), vector_attribute.cend());
            }
        }
        if (replace_nan)
            for (auto &item : result)
            {
                if (std::isnan(item))
                    item = 0.;
            }
        return result;
    }
    //differentiate current instance (after closing) with median of previous windows
    default_window_feature_vector<hit_type> diff_with_median(const std::deque<default_window_feature_vector<hit_type>> &previous_vectors)
    {
        using vector_type = std::vector<double>;
        using scalar_type = double;
        const std::string VECT_STR = "[[";
        size_t median_index = previous_vectors.size() / 2;
        default_window_feature_vector<hit_type> diff_vector;
        for (auto &attribute_name : attribute_names())
        {
            std::vector<vector_type> attribute_vectors;
            std::vector<scalar_type> attribute_scalars;
            for (auto &previous_vector : previous_vectors)
            {
                // auto attribute_value = previous_vector.get_value(attribute_name);
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
            }
            else
            {
                double current_value = std::get<scalar_type>(get_value(attribute_name));
                if (attribute_name == attribute_names()[0])
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
    //check if data is not end token
    bool is_valid() const
    {
        return hit_count >= 0;
    }
    uint32_t size() const
    {
        return avg_size();
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
    //serialize the closed window vector feature
    void write_vector(std::ofstream &stream, const std::vector<double> &vector_attribute) const
    {
        stream << "[";
        for (uint64_t i = 0; i < vector_attribute.size(); ++i)
        {
            stream << std::fixed << std::setprecision(2) << " " << vector_attribute[i];
        }
        stream << " ]";

        // return "[ " + std::accumulate(energy_distrib_str.begin(), energy_distrib_str.end(), std::string(","))  + " ]";
    }
    //deserialize the window vector feature from file
    template <typename stream_type>
    void read_vector(stream_type &stream, std::vector<double> &result)
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
            if (vector_ss >> value)
                result.push_back(value);
        }
    }
    //names of all attributes in a vector
    static std::vector<std::string> attribute_names()
    {
        return std::vector<std::string>{"start_toa[ms]", "mean_x[px]", "mean_y[px]", "std_x[px]", "std_y[px]",
                                        "hit_count[]", "total_energy[keV]", "max_energy_hit[keV]", "mean_temp_cluster_size[]", "mean_temp_cluster_energy[keV]", "e_distrib[[]]"};
    }
    //setters and getters for the features
    void set_scalar(const std::string &key, double value)
    {
        scalar_features[key] = value;
    }
    void set_vector(const std::string &key, const std::vector<double> &value)
    {
        vector_features[key] = value;
    }
    const double &get_scalar(const std::string &key) const
    {
        return scalar_features.at(key);
    }
    const std::vector<double> &get_vector(const std::string &key) const
    {
        return vector_features.at(key);
    }
    bool is_vector(const std::string &feature) const
    {
        return feature.find("[[") != std::string::npos;
    }
    //uniform getter to all features
    std::variant<double, std::vector<double>> get_value(const std::string &feature_name) const
    {
        std::variant<double, std::vector<double>> result;
        if (is_vector(feature_name))
            result = vector_features.at(feature_name);
        else
            result = scalar_features.at(feature_name);
        return result;
    }
    //add hit to all statistics of the window
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
    double time() const
    {
        return start_time_;
    }
};
//serialize the whole feature vector to a file
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
//deserialize the whole vector from a file
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
            if (scalar_feature_str == "nan")
                scalar_feature = std::numeric_limits<double>::quiet_NaN();
            else
            {
                std::replace(scalar_feature_str.begin(), scalar_feature_str.end(), '.', ',');
                scalar_feature = std::stod(scalar_feature_str);
            }
            window_feature_vect.scalar_features[feature_name] = scalar_feature;
        }
    }
    window_feature_vect.closed = true;
    return fstream;
};
