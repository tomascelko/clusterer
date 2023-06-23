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
#pragma once
class benchmarking_dataset
{
    public:
    std::vector<std::string> files {
        "../../../clusterer_data/pion/pions_180GeV_deg_0.txt",
        "../../../clusterer_data/pion/pions_180GeV_deg_30.txt",
        "../../../clusterer_data/lead/deg0_0.txt",
        "../../../clusterer_data/lead/deg90_2.txt",
        "../../../clusterer_data/neutron/beam_T0trigger_0deg_150V_10s_0.txt",
        "../../../clusterer_data/neutron/neutrons_60deg_200V_F4-W00076_1.txt",
              
    };
    std::vector<std::string> calib_files{
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076",
        "../../../clusterer_data/calib/F4-W00076"
        
    };
    std::vector<std::string> trigger_files{
        "../../../clusterer_data/trigger/trained_beam_change.nnt",
        "../../../clusterer_data/trigger/trained_beam_change.svmt",
        "../../../clusterer_data/trigger/trained_beam_change.osvmt",
    };
    std::map<std::string, std::vector<uint32_t>> aggregation = {
        {"pion", std::vector<uint32_t>{0,1}},
        {"lead", std::vector<uint32_t>{2,3}},
        {"neutron", std::vector<uint32_t>{4,5}}
    };
    void prepend_path(const std::string & prefix)
    {
        for (auto data_file : files)
        {
            data_file =  prefix + data_file;
        }    
        for (auto data_file : calib_files)
        {
            data_file =  prefix + data_file;
        }     
    } 
};
class performance_test
{
    struct statistics
    {
        double mean;
        double std;
        statistics(double mean, double std) :
        mean(mean),
        std(std){}
    };
    benchmarking_dataset dataset_;
    std::ostream * result_stream_;
    std::string output_folder_;
    bool debug_mode_;
    const std::string ini_suffix = ".ini";
    const std::string cl_suffix = "_cl.txt";
    const std::string px_suffix = "_px.txt";
    node_args n_args;
    std::vector<double> base_frequencies_;
    bool adapt_frequencies_ = false;
    public:
    performance_test(std::ostream * result_stream, bool debug_mode = false) :
    result_stream_(result_stream),
    debug_mode_(debug_mode)
    {
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
    void set_base_frequencies(node_args & args)
    {
        for (uint32_t i = 0; i < dataset_.files.size(); ++i)
        {
            auto data_file = dataset_.files[i];
            model_runner::print = false;
            model_executor executor = model_executor(std::vector<std::string>{data_file},
                                                  std::vector<std::string>{dataset_.calib_files[i]}, output_folder_);
            auto output_from_run = model_runner::run_model(model_runner::model_name::WINDOW_COMPUTER, &executor, 1, args, debug_mode_);
            
            base_frequencies_.push_back(std::stod(executor.results()[0]));
        }
    }
    statistics compute_mean_and_std(const std::vector<double> & data)
    {
        double sum = std::accumulate(data.begin(), data.end(), 0.);
        double mean = sum / data.size();
        
        double sqaured_sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.);
        double std = std::sqrt(sqaured_sum/data.size() - mean*mean);
        return statistics(mean, std);
    } 
    std::map<std::string, std::vector<double>> aggregate_results(const std::vector<std::vector<double>> & results)
    {
        std::map<std::string, std::vector<double>> aggregated_results;
        for (auto const & particle_index_pair : dataset_.aggregation)
        {
            for(auto const & particle_index : particle_index_pair.second)
            {
                if(particle_index < results.size())
                    aggregated_results[particle_index_pair.first].insert(aggregated_results[particle_index_pair.first].end(), 
                        results[particle_index].begin(), results[particle_index].end());
            }
        }
        return aggregated_results;
    }

    template<typename... other_clustering_args>
    std::vector<std::string> benchmark_dataset(model_runner::model_name model_name,  other_clustering_args... args )
    {
        std::vector<std::string> output_files;
        int64_t dataset_size = std::stoi(n_args["reader"]["repetition_count"]) *std::stoi(n_args["reader"]["repetition_size"]);
        benchmarker executor = benchmarker(dataset_.files,
                                                  dataset_.calib_files, output_folder_);
        if(adapt_frequencies_)
            executor.set_freq_scales(base_frequencies_);
        auto output_from_run = model_runner::run_model(model_name, &executor, args...);
        if (output_from_run.size() > 0)
        {
            output_files.insert(output_files.end(), output_from_run.begin(), output_from_run.end());
            
        }
        std::map<std::string, std::vector<double>> aggregated_results = aggregate_results(executor.total_exec_times_);
        for (auto const & particle_value_pair : aggregated_results)
        {
            std::vector<double> measurement_speeds(particle_value_pair.second.size(), 0);
            std::transform(particle_value_pair.second.begin(), particle_value_pair.second.end(), measurement_speeds.begin(),
             [dataset_size](auto time){
                return dataset_size/(1000 * time);
                });
            auto statistics = compute_mean_and_std(measurement_speeds);

            *result_stream_ << particle_value_pair.first << ":" << statistics.mean << " " << statistics.std << std::endl;
        }
        return output_files;
    }
    template<typename... other_clustering_args>
    std::vector<std::string> benchmark_single_file(model_runner::model_name model_name, int data_index,  other_clustering_args... args )
    {
        std::vector<std::string> output_files;
        int64_t dataset_size = std::stoi(n_args["reader"]["repetition_count"]) *std::stoi(n_args["reader"]["repetition_size"]);
        benchmarker executor = benchmarker(std::vector<std::string>{dataset_.files[data_index]},
                                                  std::vector<std::string>{dataset_.calib_files[data_index]}, output_folder_);
        if(adapt_frequencies_)
            executor.set_freq_scales(base_frequencies_);
        auto output_from_run = model_runner::run_model(model_name, &executor, args...);
        if (output_from_run.size() > 0)
        {
            output_files.insert(output_files.end(), output_from_run.begin(), output_from_run.end());
            
        }
        std::map<std::string, std::vector<double>> aggregated_results = aggregate_results(executor.total_exec_times_);
        for (auto const & particle_value_pair : aggregated_results)
        {
            std::vector<double> measurement_speeds(particle_value_pair.second.size(), 0);
            std::transform(particle_value_pair.second.begin(), particle_value_pair.second.end(), measurement_speeds.begin(),
             [dataset_size](auto time){
                return dataset_size/(1000 * time);
                });
            auto statistics = compute_mean_and_std(measurement_speeds);

            *result_stream_ << particle_value_pair.first << ":" << statistics.mean << " " << statistics.std << std::endl;
        }
        return output_files;
    }
    void test_base_models(node_args & n_args, uint32_t core_count)
    {
        model_runner::print = false;
        model_runner::recurring = true;
        
        *result_stream_ << "#Model Simple clusterer" << std::endl; 
        benchmark_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        *result_stream_ << "#Model Simple clusterer, temporal" << std::endl;
        benchmark_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::TEMPORAL);

        *result_stream_ << "#Model Simple clusterer, halo bound. box" << std::endl;
        benchmark_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::HALO_BB);

        *result_stream_ << "#Model Parallel clusterer, linear merger" << std::endl;
        benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
             
        *result_stream_ << "#Parallel clusterer, simple merger" << std::endl;
        benchmark_dataset(model_runner::model_name::PAR_SIMPLE_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);

        *result_stream_ << "#Parallel clusterer, linear merger, multioutput" << std::endl;
        benchmark_dataset(model_runner::model_name::PAR_MULTIFILE_CLUSTERER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);  
        
    }
    void test_printing_models(node_args & n_args, uint32_t core_count)
    {
        model_runner::print = true;
        model_runner::recurring = true;
        n_args["reader"]["repetition_size"] = "1000000";
        n_args["reader"]["repetition_count"] = "100";
        n_args["reader"]["freq_multiplier"] = "1";
        std::vector<std::string> output_files;
        *result_stream_ << "#Model Simple clusterer" << std::endl; 
        output_files = benchmark_dataset(model_runner::model_name::SIMPLE_CLUSTERER, core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        

        *result_stream_ << "#Model Parallel clusterer, linear merger" << std::endl;
        output_files = benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        
        *result_stream_ << "#Parallel clusterer, linear merger, multioutput" << std::endl;
        output_files = benchmark_dataset(model_runner::model_name::PAR_MULTIFILE_CLUSTERER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);  
       

    }

    void test_split_node(node_args & n_args, uint32_t core_count)
    {
        model_runner::print = false;
        model_runner::recurring = true;

        n_args["reader"]["split"] = "true";
        *result_stream_ << "#Model Parallel clusterer, split node reader" << std::endl;
        benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        
        n_args["reader"]["split"] = "false";
        n_args["calibrator"]["split"] = "true";
        *result_stream_ << "#Model Parallel clusterer, split node calibrator" << std::endl;
        
        benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        
        n_args["calibrator"]["split"] = "false";
        n_args["sorter"]["split"] = "true";
        
        *result_stream_ << "#Model Parallel clusterer, split node sorter " << std::endl;
        benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        
        n_args["sorter"]["split"] = "false";
        n_args["reader"]["split"] = "true";     

    }
    void test_merger_type(node_args & n_args, uint32_t core_count)
    {
        model_runner::print = false;
        model_runner::recurring = true;

        *result_stream_ << "#Parallel clusterer, simple merger" << std::endl;
        benchmark_dataset(model_runner::model_name::PAR_SIMPLE_MERGER,  core_count, n_args, 
                debug_mode_, model_runner::clustering_type::STANDARD);
        *result_stream_ << "#Parallel clusterer, linear merger" << std::endl;
        
        benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
        *result_stream_ << "#Parallel clusterer, tree merger" << std::endl;
        
        benchmark_dataset(model_runner::model_name::PAR_TREE_MERGER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);

        *result_stream_ << "#Parallel clusterer, linear, multioutput" << std::endl;
        
        benchmark_dataset(model_runner::model_name::PAR_MULTIFILE_CLUSTERER,  core_count, n_args, 
            debug_mode_, model_runner::clustering_type::STANDARD);
            
        
    }
    void test_n_workers(node_args & n_args)
    {
        model_runner::print = false;
        model_runner::recurring = true;

        std::vector<uint32_t> core_counts = {1,2,4,6,8,12,16,20,24};
        for (auto core_count : core_counts)
        {
            *result_stream_ << "#Model Parallel clusterer, n workers: "   << core_count << std::endl;
        
            benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
                debug_mode_, model_runner::clustering_type::STANDARD);
        }

    }
    void test_frequencies(node_args n_args, uint32_t core_count)
    {
        adapt_frequencies_ = true;
        model_runner::print = false;
        model_runner::recurring = true;
        n_args["reader"]["repetition_size"] = "1000000";
        n_args["reader"]["repetition_count"] = "1";
        n_args["reader"]["freq_multiplier"] = "1";
        set_base_frequencies(n_args);
        n_args["reader"]["repetition_size"] = "1000000";
        n_args["reader"]["repetition_count"] = "200";
        n_args["reader"]["freq_multiplier"] = "1";

        const std::vector<double> frequencies = { 0.1, 0.5, 1., 2., 4., 8., 16., 32.};
        for (double freq : frequencies)
        {
            n_args["reader"]["freq_multiplier"] = std::to_string(freq);
            *result_stream_ << "#Model Linear merger, frequency: " << freq << std::endl;
            benchmark_dataset(model_runner::model_name::PAR_LINEAR_MERGER,  core_count, n_args, 
                debug_mode_, model_runner::clustering_type::STANDARD);
        }
        adapt_frequencies_ = false;

    }
    void test_triggers(node_args n_args, uint32_t core_count)
    {
        n_args["reader"]["repetition_size"] = "1000000";
        n_args["reader"]["repetition_count"] = "200";
        n_args["reader"]["freq_multiplier"] = "1"; 
        int test_file_index = 0;
        n_args["trigger"]["use_trigger"] = "true";
        for (auto & trigger_name : dataset_.trigger_files)
        {
            n_args["trigger"]["trigger_file"] = trigger_name;
            *result_stream_ << "#Simple clusterer " << trigger_name << std::endl; 
            benchmark_single_file(model_runner::model_name::SIMPLE_CLUSTERER, test_file_index, 
                core_count, n_args, debug_mode_, model_runner::clustering_type::STANDARD);
            *result_stream_ << "#Parallel clusterer " << trigger_name << std::endl;
            benchmark_single_file(model_runner::model_name::PAR_LINEAR_MERGER, test_file_index, 
                core_count, n_args, debug_mode_, model_runner::clustering_type::STANDARD);
            *result_stream_ << "#Parallel clusterer + multioutput " << trigger_name << std::endl;
            benchmark_single_file(model_runner::model_name::PAR_MULTIFILE_CLUSTERER, test_file_index, 
                core_count, n_args, debug_mode_, model_runner::clustering_type::STANDARD);
            
        }
    }
    void run_all_benchmarks(const std::string & binary_path = "")
    {
        dataset_.prepend_path(binary_path);
        output_folder_ = binary_path + "../../output/";
        const uint64_t core_count = 4;
        node_args n_args;
        n_args["reader"]["repetition_size"] = "1000000";
        n_args["reader"]["repetition_count"] = "200";
        n_args["reader"]["freq_multiplier"] = "1";
        model_runner::print = false;
        model_runner::recurring = true;
        this->n_args = n_args;
        
        *result_stream_ << "%Testing BASE MODELS|mean throughput [Mhit/s]|model|" << std::endl;
        test_base_models(n_args, core_count);
        result_stream_->flush();
        return;
        *result_stream_ << "%Testing SPLIT NODE|mean throughput [Mhit/s]|model|" << std::endl;
        test_split_node(n_args, core_count);
        result_stream_->flush();
        
        *result_stream_ << "%Testing N WORKERS|mean throughput [Mhit/s]|n workers[]|XY_PLOTTABLE" << std::endl;
        test_n_workers(n_args);
        result_stream_->flush();
        *result_stream_ << "%Testing MERGER TYPE|mean throughput [Mhit/s]|model|" << std::endl;
        test_merger_type(n_args, core_count);
        result_stream_->flush();
        
        *result_stream_ << "%Testing FREQUENCY|mean throughput [Mhit/s]|model|XY_PLOTTABLE" << std::endl;
        test_frequencies(n_args, core_count);
        result_stream_->flush();
        
        model_runner::print = true;
        model_runner::recurring = true;
        *result_stream_ << "%Testing models with writing|mean throughput [Mhit/s]|model|" << std::endl;
        test_printing_models(n_args, core_count);
        result_stream_->flush();
        
       *result_stream_ << "%Testing trigger models|mean throughput [Mhit/s]|model|" << std::endl;
        test_triggers(n_args, core_count);
        result_stream_->flush();
    //clean_clustering_files(freq_ground_truth_names);
    }
};