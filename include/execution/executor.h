#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_package.h"

#include "../data_nodes/nodes_package.h"
#include "../mapped_mm_stream.h"
#include "../mm_stream.h"
#include "../benchmark/model_factory.h"
class model_executor
{
    std::vector<file_path> data_files_;
    file_path current_dataset_;
    std::vector<basic_path> calib_folders_;
    dataflow_controller* controller_;
    enum class calib_type
    {
        manual,
        automatic,
        same,
        none
    };
    calib_type calibration_mode_;
    void load_all_datasets(const std::string & data_folder)
    {
        std::stringstream ss;
        ss << "Loading testing datasets:" << std::endl;
        for (const auto & file: std::filesystem::directory_iterator(data_folder))
            if (!std::filesystem::is_directory(file))
            {
                ss << file.path() << std::endl;
                data_files_.emplace_back(file_path(file.path().string()));
            }
    }
    void load_all_calib_files(const std::string & calib_folder)
    {
        std::stringstream ss;
        ss << "Loading calib dirs:" << std::endl;
        for (const auto & file: std::filesystem::directory_iterator(calib_folder))
            if (std::filesystem::is_directory(file))
            {
                ss << file.path() << std::endl;
                calib_folders_.emplace_back(file_path(file.path().string()));
            }
    }
    std::string auto_find_calib_file(const file_path& data_path)
    {
        std::vector<std::string> matching_files;
        for(auto & calib_file : calib_folders_)
        {
            if(data_path.filename().find(calib_file.last_folder()) != std::string::npos)
                matching_files.push_back(calib_file.as_absolute());
        }
        switch(matching_files.size())
        {
            case 0:
                throw std::invalid_argument("no calibration file was found for file: " + data_path.as_absolute());
                break;
            case 1:
                return matching_files[0];
                break;
            default:
                throw std::invalid_argument("too many calibration files were found (ambigiouous) for file: " + data_path.as_absolute());
        }
    }
    std::vector<std::string> data_paths_as_absolute()  
    {
        std::vector<std::string> result;
        for (auto data_file : data_files_)
        {
            result.push_back(data_file.as_absolute());
        }
        return result;         
    }
    std::vector<std::string> calib_paths_as_absolute()
    {
        std::vector<std::string> result;
        for (uint32_t i = 0; i < calib_folders_.size(); ++i)
            switch(calibration_mode_)
            {
                case calib_type::automatic:

                        result.push_back(auto_find_calib_file(data_files_[i]));
                        break;
                    case calib_type::manual:
                        result.push_back(calib_folders_[i].as_absolute());
                        break;
                    case calib_type::same:
                        result.push_back(calib_folders_[0].as_absolute());
                        break;  
                    default:
                        throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                        break;
            }
        return result;
    }
    public:
    //burda file reading with calibration
    model_executor(const std::string& folder, const std::string & calib_folder): //automatic names based search of calib file
    calibration_mode_(calib_type::automatic)
    {
       load_all_datasets(folder);
       load_all_calib_files(calib_folder);
    }
    model_executor(std::vector<std::string> files, std::vector<std::string> calib_files) //

    {
        for (auto file : files)
                data_files_.emplace_back(file_path(file));
        if(calib_files.size() == 0)
            calibration_mode_ = calib_type::none;
        else
        {
            for (auto file : calib_files)
                calib_folders_.emplace_back(file_path(file));
                calibration_mode_ = calib_type::manual;
        }
    }
    
    template < typename... cl_args_type>
    void run_model(architecture_type arch, cl_args_type ... cl_args)
    {
         model_factory factory;  

        controller_ = new dataflow_controller();
        auto datasets_str = data_paths_as_absolute();
        auto calib_str = calib_paths_as_absolute();
        factory.create_model(controller_, arch,  datasets_str, calib_str, cl_args...);
        
        
        controller_->start_all();

        controller_->wait_all();
        controller_->remove_all();
        delete controller_;
        std::cout << "FINISHED" << std::endl;
            
    }
};