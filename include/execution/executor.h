#pragma once
#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_controller.h"
#include "../mm_stream.h"
#include "model_factory.h"
#include "../data_structs/node_args.h"
//class which loads inut data, creates the dataflow graph and runs the execution
class model_executor
{
    std::vector<file_path> data_files_;
    file_path current_dataset_;
    std::vector<basic_path> calib_folders_;
    dataflow_controller *controller_;
    enum class calib_type
    {
        manual,
        automatic,
        same,
        none
    };
    calib_type calibration_mode_;
    std::string output_dir_;
    //scan folder for all possible datasets
    void load_all_datasets(const std::string &data_folder)
    {
        std::stringstream ss;
        ss << "Loading testing datasets:" << std::endl;
        for (const auto &file : std::filesystem::directory_iterator(data_folder))
            if (!std::filesystem::is_directory(file))
            {
                ss << file.path() << std::endl;
                data_files_.emplace_back(file_path(file.path().string()));
            }
    }
    //scan folder for all ppossible calibration files
    void load_all_calib_files(const std::string &calib_folder)
    {
        std::stringstream ss;
        ss << "Loading calib dirs:" << std::endl;
        for (const auto &file : std::filesystem::directory_iterator(calib_folder))
            if (std::filesystem::is_directory(file))
            {
                ss << file.path() << std::endl;
                calib_folders_.emplace_back(file_path(file.path().string()));
            }
    }
    //find calib folder based on a substring match
    std::string auto_find_calib_file(const file_path &data_path)
    {
        std::vector<std::string> matching_files;
        for (auto &calib_file : calib_folders_)
        {
            if (data_path.filename().find(calib_file.last_folder()) != std::string::npos)
                matching_files.push_back(calib_file.as_absolute());
        }
        switch (matching_files.size())
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
    //convert paths from relative to absolute
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
        for (uint32_t i = 0; i < data_files_.size(); ++i)
            switch (calibration_mode_)
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
            case calib_type::none:
                break;
            default:
                throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                break;
            }
        return result;
    }

public:
    //constructiors corresponding to possible calibration modes
    model_executor(const std::string &folder, const std::string &calib_folder,
                   const std::string &output_dir) : // automatic names based search of calib file
                                                    calibration_mode_(calib_type::automatic),
                                                    output_dir_(output_dir)
    {
        load_all_datasets(folder);
        load_all_calib_files(calib_folder);
    }
    model_executor(std::vector<std::string> files, std::string calib_folder,
                   const std::string &output_dir) : output_dir_(output_dir),
                                                    calibration_mode_(calib_type::automatic)
    {
        for (auto file : files)
            data_files_.emplace_back(file_path(file));
        load_all_calib_files(calib_folder);
    }
    model_executor(std::vector<std::string> files, std::vector<std::string> calib_files,
                   const std::string &output_dir) : output_dir_(output_dir)
    {
        for (auto file : files)
            data_files_.emplace_back(file_path(file));
        if (calib_files.size() == 0)
            calibration_mode_ = calib_type::none;
        else
        {
            for (auto file : calib_files)
                calib_folders_.emplace_back(file_path(file));
            calibration_mode_ = calib_type::manual;
        }
    }
    model_executor(std::vector<std::string> files,
                   const std::string &output_dir) : output_dir_(output_dir),
                                                    calibration_mode_(calib_type::none)
    {
        for (auto file : files)
            data_files_.emplace_back(file_path(file));
    }
    std::vector<std::string> results_;
    std::vector<std::string> &results()
    {
        return results_;
    }
    //the main executor method
    //similar to benchmarker, but no time measurement 
    //and no statistical repetitions are taking place
    std::vector<std::string> run(architecture_type &&arch, const node_args &args, bool debug = false)
    {
        model_factory factory;

        controller_ = new dataflow_controller(arch.node_descriptors(), debug); // arch.node_descriptors());
        auto datasets_str = data_paths_as_absolute();
        std::vector<std::string> calib_str;

        if (calibration_mode_ != calib_type::none)
            calib_str = calib_paths_as_absolute();
        auto output_filenames = factory.create_model(controller_, arch, datasets_str, calib_str, output_dir_, args);

        std::cout << "Model running..." << std::endl;
        controller_->start_all();
        controller_->wait_all();
        results_ = controller_->results();
        controller_->remove_all();
        std::cout << "Finished the run..." << std::endl;
        delete controller_;
        return output_filenames;
    }
};
