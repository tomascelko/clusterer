#include <iostream>
#include <fstream>
#include <memory>
#include <ctime>
#include <iomanip>
#include <sstream>
class mm_write_stream
{
    std::unique_ptr<std::ofstream> cl_file_;
    std::unique_ptr<std::ofstream> px_file_;
    static constexpr std::string_view INI_SUFFIX = ".ini";
    static constexpr std::string_view CL_SUFFIX = "_cl.txt";
    static constexpr std::string_view PX_SUFFIX = "_px.txt";
    static constexpr uint32_t FLUSH_INTERVAL = 2 << 14;
    uint64_t current_line = 0;
    uint64_t current_byte = 0;
    uint64_t clusters_written_ = 0;
    uint64_t new_pixels_written_ = 0;
    std::stringstream px_buffer_;
    std::stringstream cl_buffer_;
    void open_streams(const std::string& ini_file)
    {
        std::string path_suffix = ini_file.substr(ini_file.find_last_of("\\/") + 1);
        std::string path_prefix = ini_file.substr(0, ini_file.find_last_of("\\/") + 1);
        if(ini_file.find_last_of("\\/") == std::string::npos)
        {
            //handle special case where no path is given
            path_prefix = "";
        }
        auto time = std::time(nullptr);
        auto loc_time = *std::localtime(&time);
        auto current_time = std::put_time(&loc_time, "%d-%m-%Y-%H-%M-%S");
        std::ostringstream sstream;
        sstream << path_suffix << current_time;
        std::string ini_file_name = sstream.str() + std::string(INI_SUFFIX); 
        std::string px_file_name = sstream.str() + std::string(PX_SUFFIX);
        std::string cl_file_name = sstream.str() + std::string(CL_SUFFIX);
        

        std::ofstream ini_filestream(path_prefix + ini_file_name);
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
    }  
    void close()
    {
        *cl_file_ << cl_buffer_.str();
        *px_file_ << px_buffer_.str();
        cl_file_->close();
        px_file_->close();
    }
    template <typename cluster_type>
    mm_write_stream& operator <<(const cluster_type& cluster)
    {
        cl_buffer_ << std::fixed << std::setprecision(6) << cluster.first_toa() << " "; 
        cl_buffer_ << cluster.hit_count() << " " << current_line << " " << current_byte << std::endl;
        ++clusters_written_;
        for(auto & hit : cluster.hits())
        {
            px_buffer_ << hit;
        }
        px_buffer_ << "#" << std::endl;
        current_line += cluster.hit_count() + 1;
        new_pixels_written_ += cluster.hit_count() + 1;
        current_byte = px_buffer_.tellp();
        if(clusters_written_ % FLUSH_INTERVAL == 0)
        {
            *cl_file_ << cl_buffer_.str();
            cl_buffer_.str("");
            cl_buffer_.clear();
        }
        if(new_pixels_written_ > FLUSH_INTERVAL)
        {
            *px_file_ << px_buffer_.str();
            new_pixels_written_ = 0;
            px_buffer_.str("");
            px_buffer_.clear();
        }
        return *this;
    }    

};
