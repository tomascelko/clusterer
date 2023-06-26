#include <iostream>
#include <fstream>
#include <memory>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <charconv>
#include <string_view>
#include <system_error>
#include <array>
#include "utils.h"

#pragma once
class mm_write_stream
{
    std::unique_ptr<std::ofstream> cl_file_;
    std::unique_ptr<std::ofstream> px_file_;
    static constexpr std::string_view INI_SUFFIX = ".ini";
    static constexpr std::string_view CL_SUFFIX = "_cl.txt";
    static constexpr std::string_view PX_SUFFIX = "_px.txt";
    static constexpr uint32_t FLUSH_INTERVAL = 2 << 21;
    uint64_t current_line = 0;
    uint64_t current_byte = 0;
    uint64_t clusters_written_ = 0;
    uint64_t new_pixels_written_ = 0;
    std::unique_ptr<std::stringstream> px_buffer_;
    std::unique_ptr<std::stringstream> cl_buffer_;

    void open_streams(const std::string& ini_file)
    {
        std::string path_suffix = ini_file.substr(ini_file.find_last_of("\\/") + 1);
        std::string path_prefix = ini_file.substr(0, ini_file.find_last_of("\\/") + 1);
        if(ini_file.find_last_of("\\/") == std::string::npos)
        {
            //handle special case where no path is given
            path_prefix = "";
        }

        std::ostringstream sstream;
        sstream << path_suffix;
        std::string ini_file_name = sstream.str() + std::string(INI_SUFFIX); 
        std::string px_file_name = sstream.str() + std::string(PX_SUFFIX);
        std::string cl_file_name = sstream.str() + std::string(CL_SUFFIX);
        

        std::ofstream ini_filestream(path_prefix + ini_file_name);
        if(!ini_filestream.is_open())
        {
            throw std::invalid_argument("The output location'" + path_prefix + ini_file_name + "'does not exist");
        }
        ini_filestream << "[Measurement]" << std::endl;
        ini_filestream << "PxFile=" << px_file_name << std::endl;
        ini_filestream << "ClFile=" << cl_file_name << std::endl;
        ini_filestream << "Format=txt" << std::endl;
        ini_filestream.close();

        cl_file_ = std::move(std::make_unique<std::ofstream>(path_prefix + cl_file_name));
        px_file_ = std::move(std::make_unique<std::ofstream>(path_prefix + px_file_name));
        
     
    }
    public:
    mm_write_stream(const std::string & filename)
    {
        open_streams(filename);
        cl_buffer_ = std::move(std::make_unique<std::stringstream>());
        px_buffer_ = std::move(std::make_unique<std::stringstream>());
    }  
    void close()
    {
        *cl_file_ << cl_buffer_->rdbuf();
        *px_file_ << px_buffer_->rdbuf();
        cl_file_->close();
        px_file_->close();
    }
    template <typename cluster_type>
    mm_write_stream& operator <<(const cluster_type& cluster)
    {
        //TODO write PX FILE FIRST AS A WHOLE
        *cl_buffer_ << double_to_str(cluster.first_toa()) << " "; 
        *cl_buffer_ << cluster.hit_count() << " " << current_line << " " << current_byte << "\n";
        ++clusters_written_;
        //px_buffer_.precision(6);
        for(const auto & hit : cluster.hits())
        {
            //std::array<char, 10> str;
            //std::to_chars_result res = std::to_chars((char*)(str.data()), (char*)(str.data() + str.size()), (double)90.1234);
            //px_buffer_ << hit;
            *px_buffer_ << hit;
            //px_buffer_ <<  std::to_chars(str.data(), str.data() + str.size(), 90.1234) << std::to_chars(90.1234) ;
        }
        *px_buffer_ << "#" << "\n";
        current_line += cluster.hit_count() + 1;
        new_pixels_written_ += cluster.hit_count() + 1;
        current_byte = px_buffer_->tellp() + px_file_->tellp();
        if(clusters_written_ % FLUSH_INTERVAL == 0)
        {
            *cl_file_ << cl_buffer_->rdbuf();
            cl_buffer_->clear();
            cl_buffer_->str("");
        }
        if(new_pixels_written_ > FLUSH_INTERVAL)
        {
            *px_file_ << px_buffer_->rdbuf();
            new_pixels_written_ = 0;
            px_buffer_->clear();
            px_buffer_->str("");
        }
        return *this;
    }    

};


class mm_read_stream
{
    std::unique_ptr<std::ifstream> cl_file_;
    std::unique_ptr<std::ifstream> px_file_;
    static constexpr std::string_view CL_KEY = "ClFile";
    static constexpr std::string_view PX_KEY = "PxFile";
    uint64_t current_line = 0;
    uint64_t current_byte = 0;
    uint64_t clusters_written_ = 0;
    uint64_t new_pixels_written_ = 0;
    bool calib_ = true;
    void open_streams(const std::string& ini_file)
    {
        //std::cout << ini_file << std::endl;
        std::string path_suffix = ini_file.substr(ini_file.find_last_of("\\/") + 1);
        std::string path_prefix = ini_file.substr(0, ini_file.find_last_of("\\/") + 1);
        if(ini_file.find_last_of("\\/") == std::string::npos)
        {
            //handle special case where no path is given
            path_prefix = "";
        }
        std::ifstream ini_stream(ini_file);
        std::string ini_line = "";
        std::string delimiter = "=";
        while(std::getline(ini_stream, ini_line))
        {
            auto delim_pos = ini_line.find(delimiter);
            if (delim_pos != std::string::npos)
            {
                std::string prop_key = ini_line.substr(0, delim_pos);
                std::string prop_value = ini_line.substr(delim_pos + 1);
                if(prop_key == CL_KEY)
                    cl_file_ = std::move(std::make_unique<std::ifstream>(path_prefix + prop_value));
                else if (prop_key == PX_KEY)
                    px_file_ = std::move(std::make_unique<std::ifstream>(path_prefix + prop_value));
         

            }
                
        }
        
        if(!is_open())
        {
            throw std::invalid_argument("Error, Could not open input file");
        }

    }
    public:
    mm_read_stream(const std::string & ini_filename)
    {
        open_streams(ini_filename);
    }
    bool is_open()
    {
        return cl_file_ && px_file_ && cl_file_->is_open() && px_file_->is_open();
    }
    virtual ~mm_read_stream() = default;
    void close()
    {
        cl_file_->close();
        px_file_->close();
    }
    template <typename hit_type>
    mm_read_stream& operator >>(cluster<hit_type>& cl)
    {
        cl = cluster<hit_type>();
        io_utils::skip_eol(*cl_file_);
        if (cl_file_->peek() == EOF)
        {
            cl = cluster<hit_type>::end_token();
            return *this;
        }
        double ftoa;
        uint32_t hit_count;
        uint64_t line_start, byte_start;
        if(!(*cl_file_ >> ftoa))
        {
            cl = cluster<hit_type>::end_token();
            return *this;
        }
        *cl_file_ >> hit_count >> line_start >> byte_start;
        //cl.set_byte_start(byte_start);
        //cl.set_line_start(line_start);
        cl.set_last_toa(ftoa); //is updated in .add_hit automatically
        cl.set_first_toa(ftoa);
        cl.hits().reserve(hit_count);
        short x, y;
        double toa = 0, tot = 0, e = 0; 
        for (size_t i = 0; i < hit_count; i++)
        {   
            hit_type hit;
            if (calib_)
            {
                *px_file_ >> hit;
                cl.add_hit(std::move(hit));
            }
            else
            {
                *px_file_ >> x >> y >> toa >> tot;
                cl.add_hit(hit_type{x, y, toa, tot});
            }
        }
        char hash_char;
        *px_file_ >> hash_char;
        return *this;
    }    

};
