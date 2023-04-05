#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_package.h"
#include "i_time_measurable.h"
#include "measuring_clock.h"
#include "../data_nodes/nodes_package.h"
#include "../mapped_mm_stream.h"
#include "../mm_stream.h"
#include "model_factory.h"
//TODO try more repeats
#pragma once
class benchmarker
{
    static constexpr double FREQUENCY_MULTIPLIER_ = 80.; 
    using node_id_type = std::string;
    using dataset_name_type = std::string;
    using measured_runs_type = std::vector<double>;
    using measured_dataset_matrix_type = std::map<dataset_name_type, measured_runs_type>;
    using benchmark_results_type = std::map<node_id_type, measured_dataset_matrix_type>;
    bool print_debug_info = true;
    dataflow_controller * controller_;
    std::vector<file_path> data_files_;
    file_path current_dataset_;
    std::vector<basic_path> calib_folders_;
    std::vector<std::unique_ptr<measuring_clock>> clocks_;
    benchmark_results_type results_;
    std::string output_dir_; 
    enum class calib_type
    {
        manual,
        automatic,
        same
    };
    calib_type calibration_mode_;
    private:
    bool is_text_file(const std::string & path)
    {
        if (path.size() < 4) 
            return false;
         return (path.substr(path.size() - 4) != ".txt");

    }
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
        if(print_debug_info)
            std::cout << ss.str();
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
        if(print_debug_info)
            std::cout << ss.str();
    }
    public:
    benchmarker(const std::string& folder, const std::string & calib_folder, const std::string & output_dir): //automatic names based search of calib file
    calibration_mode_(calib_type::automatic),
    output_dir_(output_dir)
    {
       load_all_datasets(folder);
       load_all_calib_files(calib_folder);
    }
    benchmarker(std::vector<std::string>& data_files, std::vector<std::string>& calib_files,
        const std::string & output_dir): //automatic names based search of calib file
    output_dir_(output_dir)
    {
        for (auto & data_file : data_files) 
        {
             data_files_.emplace_back(file_path(data_file));
        }
        switch(calib_files.size())
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
                if(calib_files.size() != data_files.size())
                    throw std::invalid_argument("calib option MANUAL was selected and number of data files is not equal to calibration files");
                for (auto & calib_file : calib_files) 
                {
                    calib_folders_.emplace_back(file_path(calib_file));
                }
            }
        }
        
        
    }
    void register_result(exec_time_result result)
    {
        //TODO WHEN MEASURING, SKIP THE READ  OPERATION
        //IDEALLY SKIP WRITE AS WELL
        results_[result.node_name()][current_dataset_.filename()].push_back(result.exec_time());
        std::cout << result.node_name() << " " << result.exec_time() << std::endl; 

    }
    /*
    template <typename clustering_type, typename ...cl_args_type>
    void prepare_model(const std::string& data_file, const std::string& calib_file, cl_args_type ... cl_args) //todo possibly replace other nodes as well
    {
        using mm_stream = mm_write_stream;
        repeating_data_reader<burda_hit>* burda_reader = new repeating_data_reader<burda_hit>{data_file, 2 << 21, FREQUENCY_MULTIPLIER_};
        //data_reader<burda_hit>* burda_reader = new data_reader<burda_hit>{data_file, 2 << 10};
        burda_to_mm_hit_adapter<mm_hit>* converter = new burda_to_mm_hit_adapter<mm_hit>(calibration(calib_file, current_chip::chip_type::size()));
        hit_sorter<mm_hit>* sorter = new hit_sorter<mm_hit>();
        clustering_type* clusterer = new clustering_type(cl_args...);
        //std::ofstream print_stream("printed_hits.txt");
        //mm_stream * print_stream = new mm_stream("/home/tomas/MFF/DT/clusterer/output/new") ;
        //data_printer<cluster<mm_hit>, mm_stream>* printer = new data_printer<cluster<mm_hit>, mm_stream>(print_stream);
        //std::ofstream* out_halo_file = new std::ofstream("/home/tomas/MFF/DT/clusterer/output/halos.txt");
        //pixel_halo_width_calculator<cluster, mm_hit, std::ofstream>  * halo_calc = new pixel_halo_width_calculator<cluster, mm_hit, std::ofstream>(out_halo_file);
        
        controller_ = new dataflow_controller();
        controller_->add_node(burda_reader);
        controller_->add_node(converter);
        controller_->add_node(sorter);
        controller_->add_node(clusterer);
        //controller_->add_node(printer);

        controller_->connect_nodes(burda_reader, converter);
        controller_->connect_nodes(converter, sorter);
        controller_->connect_nodes(sorter, clusterer);
        //controller_->connect_nodes(clusterer, printer);

    }
    template <typename clustering_type, typename split_data_type, typename ...cl_args_type>
    void prepare_model(pipe_descriptor<split_data_type>* split_descriptor, const std::string& data_file, const std::string& calib_file, cl_args_type ... cl_args) //todo possibly replace other nodes as well
    {
        using mm_stream = mm_write_stream;
        repeating_data_reader<burda_hit>* burda_reader = new repeating_data_reader<burda_hit>{data_file, 2 << 21};
        //data_reader<burda_hit>* burda_reader = new data_reader<burda_hit>{data_file, 2 << 10};
        burda_to_mm_hit_adapter<mm_hit>* converter = new burda_to_mm_hit_adapter<mm_hit>(
            calibration(calib_file, current_chip::chip_type::size()));
        cluster_merging<mm_hit>* merger = new cluster_merging<mm_hit>(split_descriptor);
        auto sorter = new hit_sorter<mm_hit>(split_descriptor);
        controller_ = new dataflow_controller();
        controller_->add_node(burda_reader);
        controller_->add_node(converter);
        controller_->add_node(sorter);
        controller_->add_node(merger);
        controller_->connect_nodes(burda_reader, converter);
        controller_->connect_nodes(converter, sorter);
        for(uint32_t i = 0; i < split_descriptor->pipe_count(); ++i)
        {
            auto clusterer = new clustering_type(cl_args...);
            controller_->add_node(clusterer);
            controller_->connect_nodes(sorter, clusterer);
            controller_->connect_nodes(clusterer, merger);
        }
        //std::ofstream print_stream("printed_hits.txt");
        //mm_stream * print_stream = new mm_stream("/home/tomas/MFF/DT/clusterer/output/new") ;
        //data_printer<cluster<mm_hit>, mm_stream>* printer = new data_printer<cluster<mm_hit>, mm_stream>(print_stream);
        //std::ofstream* out_halo_file = new std::ofstream("/home/tomas/MFF/DT/clusterer/output/halos.txt");
        //pixel_halo_width_calculator<cluster, mm_hit, std::ofstream>  * halo_calc = new pixel_halo_width_calculator<cluster, mm_hit, std::ofstream>(out_halo_file);
        

        

    }*/
    void print_results(std::ostream & stream)
    {
        for (auto & node_pair : results_)
        {
            stream << "Node name:" << node_pair.first << std::endl;
            for (auto & dataset_pair : node_pair.second)
            {
                stream << "Dataset name: " << dataset_pair.first << std::endl;
                for (uint32_t i = 0; i < dataset_pair.second.size(); ++i)
                {
                    stream << "Measured time from run " << i << ":" << dataset_pair.second[i] << std::endl;
                }
            }
        }
    }
    void run_benchmark_for_dataset()
    {
        std::cout << "Testing dataset "  << current_dataset_.filename() << std::endl;
        for (i_data_node * node : controller_->nodes())
        {
            auto measurable = dynamic_cast<i_time_measurable*>(node);
            if (measurable != nullptr) //node is measurable
            {
                auto new_clock = std::make_unique<measuring_clock>([this](exec_time_result result)
                {
                    register_result(result);
                }, true);
                clocks_.emplace_back(std::move(new_clock));
                measurable->prepare_clock(clocks_[clocks_.size()-1].get());
            }
        }
        controller_->start_all();

        controller_->wait_all();
        controller_->remove_all();
        clocks_.clear();
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
    /*template <typename clusterer_type, typename hit_type, typename... cl_arg_types>
    void run_whole_benchmark(pipe_descriptor<hit_type>* split_descr, const std::string & clustering_name, cl_arg_types... cl_args)
    {
        const uint16_t REPEATS = 1;
        for (uint32_t i = 0; i < data_files_.size(); ++i)
        {   
            for(uint32_t j = 0; j < REPEATS; j++)
            { 
            current_dataset_ = data_files_[i];
            switch(calibration_mode_)
            {
                case calib_type::automatic:
                    prepare_model<clusterer_type>(split_descr, data_files_[i].as_absolute(), auto_find_calib_file(data_files_[i]), cl_args...);
                    break;
                case calib_type::manual:
                    prepare_model<clusterer_type>(split_descr, data_files_[i].as_absolute(), calib_folders_[i].as_absolute(), cl_args...);
                    break;
                case calib_type::same:
                    prepare_model<clusterer_type>(split_descr, data_files_[i].as_absolute(), calib_folders_[0].as_absolute(), cl_args...);
                    break;  
                default:
                    throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                    break;
            }
            
            run_benchmark_for_dataset();
            delete controller_;
            std::cout << "FINISHED" << std::endl;
            }
        }
        print_results(std::cout);
    }
    template <typename clusterer_type, typename... cl_arg_types>
    void run_whole_benchmark(const std::string & clustering_name, cl_arg_types... cl_args)
    {
        const uint16_t REPEATS = 3;
        for (uint32_t i = 0; i < data_files_.size(); ++i)
        {   
            for(uint32_t j = 0; j < REPEATS; j++)
            { 
            current_dataset_ = data_files_[i];
            switch(calibration_mode_)
            {
                case calib_type::automatic:
                    prepare_model<clusterer_type>(data_files_[i].as_absolute(), auto_find_calib_file(data_files_[i]), cl_args...);
                    break;
                case calib_type::manual:
                    prepare_model<clusterer_type>(data_files_[i].as_absolute(), calib_folders_[i].as_absolute(), cl_args...);
                    break;
                case calib_type::same:
                    prepare_model<clusterer_type>(data_files_[i].as_absolute(), calib_folders_[0].as_absolute(), cl_args...);
                    break;  
                default:
                    throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                    break;
            }
            
            run_benchmark_for_dataset();
            delete controller_;
            std::cout << "FINISHED" << std::endl;
            }
        }
        print_results(std::cout);
    }
*/
    template <typename... cl_arg_types>
    void run(architecture_type arch, cl_arg_types... cl_args)
    {
    const uint16_t REPEATS = 1;

    model_factory factory;
        for (uint32_t i = 0; i < data_files_.size(); ++i)
        {   
            for(uint32_t j = 0; j < REPEATS; j++)
            { 

            controller_  = new dataflow_controller();
            current_dataset_ = data_files_[i];
            switch(calibration_mode_)
            {
                case calib_type::automatic:
                    factory.create_model(controller_, arch,  data_files_[i].as_absolute(), auto_find_calib_file(data_files_[i]), output_dir_, cl_args...);
                    break;
                case calib_type::manual:
                    factory.create_model(controller_, arch, data_files_[i].as_absolute(), calib_folders_[i].as_absolute(), output_dir_, cl_args...);
                    break;
                case calib_type::same:
                    factory.create_model(controller_, arch, data_files_[i].as_absolute(), calib_folders_[0].as_absolute(), output_dir_,  cl_args...);
                    break;  
                default:
                    throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                    break;
            }
            
            run_benchmark_for_dataset();
            delete controller_;
            std::cout << "FINISHED" << std::endl;
            }
        }
        print_results(std::cout);
    }
};