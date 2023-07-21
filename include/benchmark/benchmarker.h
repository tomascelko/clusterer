#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_package.h"
#include "i_time_measurable.h"
#include "measuring_clock.h"
#include "../data_nodes/nodes_package.h"
// #include "../mapped_mm_stream.h"
#include "../mm_stream.h"
#include "model_factory.h"

#pragma once

// class handles the run of the model, including the time measurement
class benchmarker
{
    static constexpr double FREQUENCY_MULTIPLIER_ = 1.;
    using node_id_type = std::string;
    using dataset_name_type = std::string;
    using measured_runs_type = std::vector<double>;
    using measured_dataset_matrix_type = std::map<dataset_name_type, measured_runs_type>;
    using benchmark_results_type = std::map<node_id_type, measured_dataset_matrix_type>;
    bool print_debug_info = true;
    std::unique_ptr<dataflow_controller> controller_;
    std::vector<file_path> data_files_;
    file_path current_dataset_;
    std::vector<basic_path> calib_folders_;
    std::vector<std::unique_ptr<measuring_clock>> clocks_;
    benchmark_results_type results_;
    std::string output_dir_;
    std::vector<double> freq_scales_;
    //the algorithm ehich should be used for calibration
    enum class calib_type
    {
        manual, //each data file has a corresponding calib type
        automatic, //the calib file for each is found on pattern matching
        same //a single calib file is used for all datasets
    };
    calib_type calibration_mode_;

private:

    //check if selected path has a .txt suffix
    bool is_text_file(const std::string &path)
    {
        if (path.size() < 4)
            return false;
        return (path.substr(path.size() - 4) != ".txt");
    }
    //load all .txt file datasets in a folder
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
        if (print_debug_info)
            std::cout << ss.str();
    }
    //load calibration files from a folder
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
        if (print_debug_info)
            std::cout << ss.str();
    }

public:
    benchmarker(const std::string &folder, const std::string &calib_folder, const std::string &output_dir) : // automatic names based search of calib file
                                                                                                             calibration_mode_(calib_type::automatic),
                                                                                                             output_dir_(output_dir)
    {
        load_all_datasets(folder);
        load_all_calib_files(calib_folder);
    }
    benchmarker(const std::vector<std::string> &data_files, const std::string &calib_folder, const std::string &output_dir) : // automatic names based search of calib file
                                                                                                                              calibration_mode_(calib_type::automatic),
                                                                                                                              output_dir_(output_dir)
    {
        for (auto &data_file : data_files)
        {
            data_files_.emplace_back(file_path(data_file));
        }
        load_all_calib_files(calib_folder);
    }
    benchmarker(const std::vector<std::string> &data_files, const std::vector<std::string> &calib_files,
                const std::string &output_dir) : // automatic names based search of calib file
                                                 output_dir_(output_dir)
    {
        for (auto &data_file : data_files)
        {
            data_files_.emplace_back(file_path(data_file));
        }
        switch (calib_files.size())
        {
        case 0:
        {
            throw std::invalid_argument("no calibration files passed");
        }
        case 1:
        {
            calibration_mode_ = calib_type::same;
            calib_folders_.emplace_back(file_path(calib_files[0]));
        }
        default:
        {
            calibration_mode_ = calib_type::manual;
            if (calib_files.size() != data_files.size())
                throw std::invalid_argument("calib option MANUAL was selected and number of data files is not equal to calibration files");
            for (auto &calib_file : calib_files)
            {
                calib_folders_.emplace_back(file_path(calib_file));
            }
        }
        }
    }
    //a callback used to return result from a run
    void register_result(exec_time_result result)
    {
        results_[result.node_name()][current_dataset_.filename()].push_back(result.exec_time());
        std::cout << result.node_name() << " " << result.exec_time() << std::endl;
    }
    //writes the benchmark result to a file
    void print_results(std::ostream &stream)
    {
        for (auto &node_pair : results_)
        {
            stream << "Node name:" << node_pair.first << std::endl;
            for (auto &dataset_pair : node_pair.second)
            {
                stream << "Dataset name: " << dataset_pair.first << std::endl;
                for (uint32_t i = 0; i < dataset_pair.second.size(); ++i)
                {
                    stream << "Measured time from run " << i << ":" << dataset_pair.second[i] << std::endl;
                }
            }
        }
        for (uint32_t i = 0; i < data_files_.size(); ++i)
        {
            stream << "Data File:" << data_files_[i].filename() << std::endl;
            for (uint32_t j = 0; j < statistic_repeats_; ++j)
            {
                stream << "Time: " << total_exec_times_[i][j] << "ms";
            }
            stream << std::endl;
        }
    }
    //runs all nodes using the controller node
    void run_benchmark_for_dataset()
    {
        // std::cout << "Testing dataset "  << current_dataset_.filename() << std::endl;
        for (i_data_node *node : controller_->nodes())
        {
            auto measurable = dynamic_cast<i_time_measurable *>(node);
            if (measurable != nullptr) // node is measurable
            {
                auto new_clock = std::make_unique<measuring_clock>([this](exec_time_result result)
                                                                   { register_result(result); },
                                                                   true);
                clocks_.emplace_back(std::move(new_clock));
                measurable->prepare_clock(clocks_[clocks_.size() - 1].get());
            }
        }
        controller_->start_all();

        controller_->wait_all();
        controller_->remove_all();
        clocks_.clear();
    }
    //used for pattern match finding of a calibration - the calib folder name
    //must be a suffix of the dataset name
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

    std::vector<std::vector<double>> total_exec_times_;
    uint32_t statistic_repeats_ = 5;

    //removes the files creates during the benchmark run
    void clean_clustering_files(const std::vector<std::string> &files)
    {
        const std::string ini_suffix = ".ini";
        const std::string cl_suffix = "_cl.txt";
        const std::string px_suffix = "_px.txt";
        for (const auto &file : files)
        {
            std::remove((file + ini_suffix).c_str());
            std::remove((file + cl_suffix).c_str());
            std::remove((file + px_suffix).c_str());
        }
    }
    // std::vector<uint32_t> buffer_repetitions_ = {300};//{1, 3, 5, 7, 10, 15, 20, 30, 50, 100, 200};
    // stores the base hitrates of the data files so we can correctly scale them
    void set_freq_scales(const std::vector<double> &freq_scales)
    {
        freq_scales_ = freq_scales;
    }
    // the main method of benchmarker, runs the model multiple times with statisical repeats
    // and computes the true frequency multipler, if any
    std::vector<std::string> run(architecture_type &&arch, node_args &args, bool debug = false)
    {

        model_factory factory;
        std::vector<std::string> output_filenames;
        double freq_multiplier;
        if (args["reader"].count("freq_multiplier") > 0)
        {
            freq_multiplier = std::stod(args["reader"]["freq_multiplier"]);
            // std::cout << "FREQ multipl" << freq_multiplier << std::endl;
        }
        controller_ = std::make_unique<dataflow_controller>(arch.node_descriptors(), debug);
        for (uint32_t i = 0; i < data_files_.size(); ++i)
        {

            total_exec_times_.emplace_back(std::vector<double>());

            for (uint32_t j = 0; j < statistic_repeats_; j++)
            {
                current_dataset_ = data_files_[i];
                std::vector<std::string> temporary_output;
                if (freq_scales_.size() > 0)
                {
                    args["reader"]["freq_multiplier"] = std::to_string((freq_multiplier / (freq_scales_[i] / 1000000)));
                    std::cout << "FINAL FREQ" << args["reader"]["freq_multiplier"] << std::endl;
                }
                switch (calibration_mode_)
                {
                case calib_type::automatic:
                    temporary_output = factory.create_model(controller_.get(), arch, data_files_[i].as_absolute(), auto_find_calib_file(data_files_[i]), output_dir_, args);
                    break;
                case calib_type::manual:
                    temporary_output = factory.create_model(controller_.get(), arch, data_files_[i].as_absolute(), calib_folders_[i].as_absolute(), output_dir_, args);
                    break;
                case calib_type::same:
                    temporary_output = factory.create_model(controller_.get(), arch, data_files_[i].as_absolute(), calib_folders_[0].as_absolute(), output_dir_, args);
                    break;
                default:
                    throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                    break;
                }
                if (temporary_output.size() > 0)
                    output_filenames.insert(output_filenames.end(), temporary_output.begin(), temporary_output.end());

                auto start_time = std::chrono::high_resolution_clock::now();
                run_benchmark_for_dataset();
                auto end_time = std::chrono::high_resolution_clock::now();
                clean_clustering_files(temporary_output);
                total_exec_times_.back().push_back(
                    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
            }
            
        }
        controller_.reset();

        return output_filenames;
    }
};