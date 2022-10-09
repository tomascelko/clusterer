#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_package.h"
#include "i_time_measurable.h"
#include "measuring_clock.h"
#include "../data_nodes/nodes_package.h"


//TODO try more repeats
#pragma once
class benchmarker
{
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
    benchmarker(const std::string& folder, const std::string & calib_folder): //automatic names based search of calib file
    calibration_mode_(calib_type::automatic)
    {
       load_all_datasets(folder);
       load_all_calib_files(calib_folder);
    }
    benchmarker(std::vector<std::string>& data_files, std::vector<std::string>& calib_files) //automatic names based search of calib file
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
    template <typename clustering_type>
    void prepare_model(clustering_type * clusterer, const std::string& data_file, const std::string& calib_file) //todo possibly replace other nodes as well
    {
        data_reader<burda_hit>* burda_reader = new data_reader<burda_hit>{data_file, 2 << 10};
        burda_to_mm_hit_adapter<mm_hit>* converter = new burda_to_mm_hit_adapter<mm_hit>(current_chip::chip_type::size(), calibration(calib_file, current_chip::chip_type::size()));
        hit_sorter<mm_hit>* sorter = new hit_sorter<mm_hit>();
        //std::ofstream print_stream("printed_hits.txt");
        //mm_write_stream * print_stream = new mm_write_stream("/home/tomas/MFF/DT/clusterer/output/testing_new_merge") ;
        //data_printer<cluster<mm_hit>, mm_write_stream>* printer = new data_printer<cluster<mm_hit>, mm_write_stream>(print_stream);
        
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
                matching_files.push_back(calib_file.last_folder());
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
    void run_whole_benchmark()
    {
        using parallel_clusterer_type = parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>, temporal_clustering_descriptor<mm_hit>>;
        
        const uint16_t REPEATS = 3;
        for (uint32_t i = 0; i < data_files_.size(); ++i)
        {   
            for(uint32_t j = 0; j < REPEATS; j++){ 
            pixel_list_clusterer<cluster>* clusterer = new pixel_list_clusterer<cluster>();
            energy_filtering_clusterer<mm_hit>* e_clusterer = new energy_filtering_clusterer<mm_hit>();
            parallel_clusterer_type* p_clusterer = new parallel_clusterer_type(
                temporal_clustering_descriptor<mm_hit>(2)); 
            current_dataset_ = data_files_[i];
            switch(calibration_mode_)
            {
                case calib_type::automatic:
                    prepare_model(p_clusterer, data_files_[i].as_absolute(), auto_find_calib_file(data_files_[i]));
                    break;
                case calib_type::manual:
                    prepare_model(p_clusterer, data_files_[i].as_absolute(), calib_folders_[i].as_absolute());
                    break;
                case calib_type::same:
                    prepare_model(p_clusterer, data_files_[i].as_absolute(), calib_folders_[0].as_absolute());
                    break;    
                default:
                    throw std::invalid_argument("invalid calibration type (choose one of auto/manual/same)");
                    break;
            }
            
            run_benchmark_for_dataset();
            std::cout << "FINISHED" << std::endl;
            delete controller_;
            }
        }
        print_results(std::cout);
    }



};