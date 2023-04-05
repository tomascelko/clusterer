#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <utility>
#pragma once
//TODO implement << operator ??? and then also is valid (define invalid cluster)
template <typename data_type>
class cluster
{
protected:
    double first_toa_ = std::numeric_limits<double>::max();
    double last_toa_ = - std::numeric_limits<double>::max();
    //uint64_t line_start_;
    uint64_t hit_count_;
    //uint64_t byte_start_;
    std::vector<data_type> hits_;
    
public:

    static constexpr uint64_t avg_size()
    {
        return 20 * data_type::avg_size();
    }

    struct first_toa_comparer
    {
        auto operator() (const cluster& left, const cluster& right) const
        {
            if(left.first_toa() < right.first_toa())
                return false;
            if(right.first_toa() < left.first_toa())
                return true;
            return false;   
        
        }
        auto operator() (cluster& left, cluster& right) const
        {
            if(left.first_toa() < right.first_toa())
                return false;
            if(right.first_toa() < left.first_toa())
                return true;
            return false;   
        
        }
    };
    static cluster<data_type> end_token()
    {
        cluster<data_type>cl{};
        cl.first_toa_ = LONG_MAX;
        return cl;
    }

    cluster() :
    hit_count_(0)
    { }
    bool is_valid() const
    {
        return hits_.size() > 0;
    }
    virtual ~cluster() = default;
    
    double first_toa() const
    { 
        return first_toa_;
    }
    double last_toa() const
    { 
        return last_toa_;
    }
    
    /*uint64_t line_start() const
    {
        return line_start_;
    }*/
    uint64_t hit_count() const
    {
        return hits().size();
    }
    /*uint64_t byte_start() const
    {
        return byte_start_;
    }*/
    std::vector<data_type>& hits()
    {
        return hits_;
    }
    const std::vector<data_type>& hits() const
    {
        return hits_;
    }
    void add_hit(data_type && hit)
    {
        if(hit.toa() < first_toa_)
            first_toa_ = hit.toa();
        if(hit.toa() > last_toa_)
            last_toa_ = hit.toa();
        hits_.emplace_back(hit);
        ++hit_count_; 
    }
    double tot_energy()
    {
        double tot_energy;
        for (uint32_t i = 0; i < hits_.size(); i++)
        {
            tot_energy += hits_[i].e();
        }
        return tot_energy;
    }
    /*void set_byte_start(uint64_t new_byte_start)
    {
        byte_start_ = new_byte_start;
    }
    void set_line_start(uint64_t new_line_start)
    {
        line_start_ = new_line_start;
    }*/
    void set_first_toa(double toa)
    {
        first_toa_ = toa;
    }
    void set_last_toa(double toa)
    {
        last_toa_ = toa;
    }
    uint64_t size()
    {
        return hits_.size() * data_type::avg_size() + 2 * sizeof(double) + 3 * sizeof(uint64_t);
    }
    /*virtual void write(std::ofstream* cl_stream, std::ofstream* px_stream) const
    {
        //std::stringstream result;
        *px_stream << std::fixed << std::setprecision(6) << first_toa_ << " " << hit_count_<< std::string(" ") << line_start_ << " " << byte_start_ << std::endl;
        for (uint32_t i = 0; i < hits_.size(); i++)
        {
            *px_stream << hits_[i];
        }
        *px_stream << "#" << std::endl;
        //return result.str();
    }*/
    virtual std::pair<double, double> center()
    {
        double mean_x = 0;
        double mean_y = 0;
    
        for (uint32_t i = 0; i < hits_.size(); i++)
        {
            mean_x += hits_[i].x();
            mean_y += hits_[i].y();    
        } 
        return std::make_pair<double, double>(mean_x / hits_.size(), mean_y / hits_.size());
    }
    void merge_with(cluster<data_type> && other)
    {
        hits().reserve(hits().size() + other.hits().size());
        hits().insert(hits().end(), std::make_move_iterator(other.hits().begin()),
        std::make_move_iterator(other.hits().end()));
        set_first_toa(std::min(first_toa(), other.first_toa()));
        set_last_toa(std::max(last_toa(), other.last_toa()));
    }
    bool approx_equals(cluster<data_type> & other)
    {
        if(other.hits().size() != hits().size())
        {
            return false;
        }
        auto hit_comparer = [](const data_type & left, const data_type & right){
            if(left.toa() < right.toa())
                return true;
            if(left.toa() > right.toa())
                return false;
            if (left.x() < right.x())
                return true;
            if (left.x() > right.x())
                return false;
            if (left.y() < right.y())
                return true;
            return false;

        };
        std::sort(hits().begin(), hits().end(), hit_comparer);
        std::sort(other.hits().begin(), other.hits().end(), hit_comparer);
        
        for (uint32_t i = 0; i < hits().size(); ++i)
        {
            if (!hits()[i].approx_equals(other.hits()[i]))
                return false;
        }
        return true;
    }
};