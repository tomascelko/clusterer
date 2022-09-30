#include "../utils.h"
#include <vector>
#include <string>
#include <iomanip>
#include <string_view>
#include <iostream>
#include <cmath>
class calibration
{
    std::vector<std::vector<double>> a_;
    std::vector<std::vector<double>> b_;
    std::vector<std::vector<double>> c_;
    std::vector<std::vector<double>> t_;
    static constexpr std::string_view a_suffix = "a.txt";
    static constexpr std::string_view b_suffix = "b.txt";
    static constexpr std::string_view c_suffix = "c.txt";
    static constexpr std::string_view t_suffix = "t.txt";
    
    void load_calib_vector(std::string&& name, const coord& chip_size, std::vector<std::vector<double>> & vector)
    {
        std::ifstream calib_stream (name);
        vector.reserve(chip_size.x());
        for (uint32_t i = 0; i < chip_size.x(); i++)
        {
            std::vector<double> line;
            line.reserve(chip_size.y());
            for (uint32_t j = 0; j < chip_size.y(); j++)
            {
                double x;
                calib_stream >> x;
                line.push_back(x);
            }
            vector.emplace_back(std::move(line));    
        }
    }
    public: 
    calibration(const std::string& calib_folder, const coord& chip_size)
    {
        //TODO make class path to encapsulate string and abstract between unix and windows paths / \\ 

        load_calib_vector(calib_folder + std::string(a_suffix), chip_size, a_);
        load_calib_vector(calib_folder + std::string(b_suffix), chip_size, b_); 
        load_calib_vector(calib_folder + std::string(c_suffix), chip_size, c_); 
        load_calib_vector(calib_folder + std::string(t_suffix), chip_size, t_);        
    }
   
    double compute_energy(short x, short y, double tot)
    {
        double a = a_[y][x];
        double c = c_[y][x];
        double t = t_[y][x];
        double b = b_[y][x];

        double a2 = a;
        double b2 = (-a*t+b-tot);
        double c2 = tot*t - b*t - c;
        return (-b2 + std::sqrt(b2*b2 - 4*a2*c2))/(2*a2); //greater root of quad formula
    }
};