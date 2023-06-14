#include "../../data_structs/burda_hit.h"
#include "../../data_flow/dataflow_controller.h"
#include "../../data_nodes/nodes_package.h"
#include "../../data_structs/cluster.h"
#include "../../data_structs/node_args.h"
#include "../../mm_stream.h"
#include "../../utils.h"
#include "../../benchmark/i_time_measurable.h"
#include "../../benchmark/benchmarker.h"
#include "../../execution/executor.h"
#include "../../execution/model_runner.h"
#include <iostream>
#include <fstream>
#include <cstdio>

class clustering_dataset
{
    public:
    std::vector<std::string> validation {
        "../../../clusterer_data/pion/pions_180GeV_deg_0.txt",
        "../../../clusterer_data/pion/pions_180GeV_deg_30.txt",
        "../../../clusterer_data/lead/deg0_0.txt",
        "../../../clusterer_data/lead/deg90_2.txt",
        "../../../clusterer_data/neutron/beam_T0trigger_0deg_150V_10s_0.txt",
        "../../../clusterer_data/neutron/neutrons_60deg_200V_F4-W00076_1.txt",
        
        
    };
    const std::vector<std::string> validation_ground_truth{
        "../../../clusterer_data/ground_truth/pion/Clustered - pions_180GeV_deg_0.txt - 20-59 11-06-2023.ini",
        "../../../clusterer_data/ground_truth/pion/Clustered - pions_180GeV_deg_30.txt - 20-59 11-06-2023.ini",
        "../../../clusterer_data/ground_truth/lead/Clustered - deg0_0.txt - 22-44 10-06-2023.ini",
        "../../../clusterer_data/ground_truth/lead/Clustered - deg90_2.txt - 22-45 10-06-2023.ini",
        "../../../clusterer_data/ground_truth/neutron/Clustered - beam_T0trigger_0deg_150V_10s_0.txt - 22-45 10-06-2023.ini",
        "../../../clusterer_data/ground_truth/neutron/Clustered - neutrons_60deg_200V_F4-W00076_1.txt - 20-42 11-06-2023.ini"
    };  
    std::map<std::string, std::vector<uint32_t>> aggregation = {
        {"pion", std::vector<uint32_t>{0,1}},
        {"lead", std::vector<uint32_t>{2,3}},
        {"neutron", std::vector<uint32_t>{4, 5}}
    };
    std::vector<std::string> calib_files{
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076"
    };
    void prepend_path(const std::string & prefix)
    {
        for (auto data_file : validation)
        {
            data_file =  prefix + data_file;
        }
        for (auto data_file : validation_ground_truth)
        {
            data_file =  prefix + data_file;
        }
        for (auto & calib_file : calib_files)
        {
            calib_file =  prefix + calib_file;
        }
    } 
};
class validation
{
    struct statistics
    {
        double mean;
        double std;
        statistics(double mean, double std) :
        mean(mean),
        std(std){}
    };
    clustering_dataset dataset_;
    std::ostream * result_stream_;
    std::string output_folder_;
    bool debug_mode_;
    const std::string ini_suffix = ".ini";
    const std::string cl_suffix = "_cl.txt";
    const std::string px_suffix = "_px.txt";
    std::string output_name_;
    bool scale_frequency_ = false;
    std::vector<double> base_frequencies_;
    public:
    validation(std::ostream * result_stream, const std::string & output_file = "", bool debug_mode = false) :
    result_stream_(result_stream),
    debug_mode_(debug_mode),
    output_name_(output_file)
    {

    }
    
    statistics compute_mean_and_std(const std::vector<double> & data)
    {
        double sum = std::accumulate(data.begin(), data.end(), 0.);
        double mean = sum / data.size();
        
        double sqaured_sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.);
        double std = std::sqrt(sqaured_sum/data.size() - mean*mean);
        return statistics(mean, std);
    } 

    std::map<std::string, std::vector<double>> aggregate_results(const std::vector<double> & results)
    {
        std::map<std::string, std::vector<double>> aggregated_results;
        for (auto const & particle_index_pair : dataset_.aggregation)
        {
            for(auto const & particle_index : particle_index_pair.second)
            {
                aggregated_results[particle_index_pair.first].push_back(results[particle_index]);
            }
        }
        return aggregated_results;
    }


    std::vector<std::string> add_suffix(const std::vector<std::string> & file_names, std::string suffix= ".ini")
    {
        std::vector<std::string> new_file_names;
        new_file_names.reserve(file_names.size());
        for (const auto & file_name : file_names)
            new_file_names.push_back(file_name + suffix);
        return new_file_names;
    }
    void clean_clustering_files(const std::vector<std::string> & files)
    {
        for (const auto & file : files)
        {
            std::remove((file + ini_suffix).c_str());
            std::remove((file + cl_suffix).c_str());
            std::remove((file + px_suffix).c_str());
        }
    }
    void validate_pairs(const std::vector<std::string>& results, const std::vector<std::string>& ground_truth, node_args & node_args, bool remove = true)
    {
        //return;
        std::vector<double> ious;
        for (uint32_t i = 0; i < ground_truth.size(); i++)
        {

            model_executor executor = model_executor(std::vector<std::string>{results[i] + ini_suffix, ground_truth[i]},
                                                   "");
            model_runner::run_model(model_runner::model_name::VALIDATION, &executor, 1, node_args, debug_mode_);    
            double iou = std::stod(executor.results()[0]);
            ious.push_back(iou);
            
        }
        auto aggregated_results = aggregate_results(ious);
        for (auto const & particle_ious_pair : aggregated_results)
        {
            auto statistics = compute_mean_and_std(particle_ious_pair.second);
            *result_stream_ << particle_ious_pair.first << ":" << statistics.mean << " " << statistics.std << std::endl;
        }
        if (remove)
            clean_clustering_files(results);

    }
    template<typename... other_clustering_args>
    std::vector<std::string> cluster_dataset(model_runner::model_name model_name, uint32_t core_count, node_args &  n_args, other_clustering_args... args )
    {
        std::vector<std::string> output_files;
        double freq =  std::stod(n_args["reader"]["freq_multiplier"]);
        for (uint32_t i = 0; i < dataset_.validation.size(); ++i)
        {
            auto data_file = dataset_.validation[i];
            if (scale_frequency_)
            {
                n_args["reader"]["freq_multiplier"] =  std::to_string(freq/(base_frequencies_[i] / 1000000));
                std::cout << "True multiplier:" << n_args["reader"]["freq_multiplier"] << std::endl;
            }
            model_executor executor = model_executor(std::vector<std::string>{data_file},
                                                  std::vector<std::string>{dataset_.calib_files[i]}, output_folder_);
            auto output_from_run = model_runner::run_model(model_name, &executor, core_count, n_args, args...);
            output_files.insert(output_files.end(), output_from_run.begin(), output_from_run.end());
        }
        return output_files;
    }

    void set_base_frequencies(node_args & args)
    {
        for (uint32_t i = 0; i < dataset_.validation.size(); ++i)
        {
            auto data_file = dataset_.validation[i];
            model_runner::print = false;
            model_executor executor = model_executor(std::vector<std::string>{data_file},
                                                  std::vector<std::string>{dataset_.calib_files[i]}, output_folder_);
            auto output_from_run = model_runner::run_model(model_runner::model_name::WINDOW_COMPUTER, &executor, 1, args, debug_mode_);
            
            base_frequencies_.push_back(std::stod(executor.results()[0]));
            
        }
    }

    void validate_base_models(node_args & n_args, uint32_t core_count)
    {
        n_args["reader"]["repetition_size"] = "-1";
        n_args["reader"]["repetition_count"] = "1";
        n_args["reader"]["freq_multiplier"] = "1";
        

        //for (int i =0; i < 10; i++)
        //{
        *result_stream_ << "#Simple clusterer" << std::endl;
        validate_pairs(cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
        debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
        //}
        //return;
        /*result_stream_ << "#Parallel clusterer, simple merger" << std::endl;
        validate_pairs(cluster_dataset(model_runner::model_name::PAR_SIMPLE_MERGER, core_count, n_args, 
        debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
        */

        *result_stream_ << "#Parallel clusterer, tree merger" << std::endl;
        validate_pairs(cluster_dataset(model_runner::model_name::PAR_TREE_MERGER, core_count, n_args, 
        debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
        
        *result_stream_ << "#Parallel clusterer, linear merger" << std::endl;
        validate_pairs(cluster_dataset(model_runner::model_name::PAR_LINEAR_MERGER, core_count, n_args, 
        debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
        
        *result_stream_ << "#Simple halo BB clusterer" << std::endl;
        validate_pairs(cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
        debug_mode_, model_runner::clustering_type::HALO_BB), dataset_.validation_ground_truth, n_args);
        

    }

    void validate_temporal_clustering(node_args & n_args, uint32_t core_count)
    {
        const std::vector<double> frequencies = { 0.05, 0.1, 0.5, 1., 2., 4., 8., 16., 32.};
        scale_frequency_ = true;
        for(uint32_t i = 0; i < frequencies.size(); ++i)
        {
            double freq = frequencies[i];
            *result_stream_ << "#Model data frequency :" << freq << std::endl;
            std::cout << "FREQUENCY BASE " << freq << std::endl;
            n_args["reader"]["freq_multiplier"] = std::to_string(freq);
            
            auto freq_ground_truth_names = cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
            auto freq_ground_truth = add_suffix(freq_ground_truth_names);

            n_args["reader"]["freq_multiplier"] = std::to_string(freq);
            validate_pairs(cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::TEMPORAL), freq_ground_truth, n_args);


            clean_clustering_files(freq_ground_truth_names);
        }
        scale_frequency_ = false;
    }

    void validate_halo_bb_clustering(node_args & n_args, uint32_t core_count)
    {
        const std::vector<double> frequencies = { 0.05, 0.1, 0.5, 1., 2., 4., 8., 16., 32.};
        scale_frequency_ = true;
        for(uint32_t i = 0; i < frequencies.size(); ++i)
        {
            double freq = frequencies[i];
            *result_stream_ << "#Model data frequency :" << freq << std::endl;
            std::cout << "FREQUENCY BASE " << freq << std::endl;
            n_args["reader"]["freq_multiplier"] = std::to_string(freq);
            
            auto freq_ground_truth_names = cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
            auto freq_ground_truth = add_suffix(freq_ground_truth_names);

            n_args["reader"]["freq_multiplier"] = std::to_string(freq);
            validate_pairs(cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::HALO_BB), freq_ground_truth, n_args);

            
            clean_clustering_files(freq_ground_truth_names);
        }
        scale_frequency_ = false;
    }

    std::string binary_path_;
    void run_validation(const std::string & binary_path = "")
    {
        output_folder_ = binary_path + "../../output/benchmark_results/data/";
        std::filesystem::remove_all(output_folder_);
        std::filesystem::create_directory(output_folder_);
        //output_folder_ += "/";
        binary_path_ = binary_path;
        dataset_.prepend_path(binary_path);
        const uint64_t core_count = 4;
        node_args n_args;
        
        set_base_frequencies(n_args);
        
        for(const auto & freq : base_frequencies_)
        {
            std::cout << "MEASURED_FREQ" << freq << std::endl;
        }
        model_runner::print = true;
        model_runner::recurring = false;
        
        /*validate_pairs(cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
        
        validate_pairs(cluster_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::TEMPORAL), dataset_.validation_ground_truth, n_args);
        

        validate_pairs(cluster_dataset(model_runner::model_name::PAR_SIMPLE_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
        validate_pairs(cluster_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD), dataset_.validation_ground_truth, n_args);
    */
        model_runner::recurring = true;
        n_args["reader"]["repetition_size"] = "-1";
        n_args["reader"]["repetition_count"] = "1";
        n_args["reader"]["freq_multiplier"] = "1";
        /**result_stream_ << "%Validation of halo bounding box clustering models|Iou match[]|hit frequency [MHit/s]|XY_PLOTTABLE" << std::endl;
        //validate_halo_bb_clustering(n_args,core_count);
        
        n_args["reader"]["repetition_size"] = "-1";
        n_args["reader"]["repetition_count"] = "1";
        n_args["reader"]["freq_multiplier"] = "1";
        */
        //*result_stream_ << "%Validation of clustering models|Iou match[]|model type" <<std::endl;
        //validate_base_models(n_args, core_count);
        //return
        *result_stream_ << "%Validation of temporal clustering models|Iou match[]|hit frequency [MHit/s]|XY_PLOTTABLE" << std::endl;
        validate_temporal_clustering(n_args,core_count);



    }
};