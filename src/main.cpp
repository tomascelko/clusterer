#include "../include/data_structs/burda_hit.h"
#include "../include/data_flow/dataflow_controller.h"
#include "../include/data_nodes/nodes_package.h"
#include "../include/data_structs/cluster.h"
#include "../include/data_structs/node_args.h"
#include "../include/mm_stream.h"
#include "../include/utils.h"
#include "../include/benchmark/i_time_measurable.h"
#include "../include/benchmark/benchmarker.h"
#include "../include/execution/executor.h"
#include "../include/execution/model_runner.h"
#include "../include/experiments/validation/validation.h"
#include "../include/experiments/performance/performance_test.h"
#include "../include/execution/arg_parser.h"
#include <iostream>
#include <fstream>

// #include <cppflow/cppflow.h>


int main(int argc, char **argv)
{
    //testOnnx();
    const uint32_t TILE_SIZE = 2;
    const uint32_t TIME_WINDOW_SIZE = 50000000; // 50ms
    std::vector<std::string> args(argv, argc + argv);
    //uint32_t core_count = args.size() > 0 ? std::stoi(args[0]) : 4;
    //args = {"", "--mode", "benchmark", "--args", "../../../clusterer_data/parameters/node_params.txt"};// "--calib", "../../../clusterer_data/calib/F4-W00076/", "../../../clusterer_data/lead/deg0_0.txt"};
    std::cout << args[0] << std::endl;
    argument_parser parser = argument_parser(args[0]);
    parser.try_parse_and_run(std::vector<std::string>(args.begin() + 1, args.end()));
    /*
    std::string data_folder = args[0];
    std::vector<std::string> cluster_comparison_datasets = {
        "/home/tomas/MFF/DT/clusterer/output/clustered_0_06-05-2023_19_44_5706-05-2023-19-44-57.ini",
        "/home/tomas/MFF/DT/clusterer/test_data/validation/small/\
Clustered - pions_180GeV_bias200V_60deg_alignment_test_F4-W00076_1.txt - 19-47 28-12-2022.ini"};
    std::string calib_folder = args[1];
    std::string arch_type = args[2];
    //uint16_t clustering_cores = std::stoi(args[3]);

    std::string output_folder = "../../output/";
    std::string data_file = "../../../clusterer_data/pion/pions_180GeV_deg_0.txt";
    std::string lead_file = "../../../fieldData/lead/deg30_10.txt";
    std::string neutron_file1 = "../../../fieldData/neutron/beam_T0trigger_0deg_150V_10s_0.txt";
    std::string calib_file = "../../test_data/calib/F4-W00076/";
    node_args n_args;*/
    //n_args["trigger"]["trigger_file"] = "../../test_data/trigger/interval/test.ift";
    //n_args["trigger"]["trigger_file"] =  "../../test_data/trigger/svm/valid_pipe_svm.svmt";
    //n_args["trigger"]["data_file"] = "../../test_data/trigger/nn/window_features_test.wf";
    
   

    /*benchmarker *bench = new benchmarker(std::vector<std::string>{neutron_file1}, std::vector<std::string>{calib_file}, "../../output/");
      //model_executor * comp_executor = new model_executor(cluster_comparison_datasets,
    */  //std::vector<std::string>(), output_folder);
    /*model_executor *executor = new model_executor(std::vector<std::string>{data_file},
                                                  std::vector<std::string>{calib_file}, output_folder);


    model_runner::print = false;
    model_runner::recurring = true;
    std::cout << "selecting model" << std::endl;*/
    //model_runner::run_model(model_runner::model_name::SIMPLE_CLUSTERER, executor, 
    //    core_count, n_args, false, model_runner::clustering_type::STANDARD);
    /*
    n_args["trigger"]["use_trigger"] = "true";
    n_args["trigger"]["trigger_file"]= "../../../clusterer_data/trigger/trained_beam_change.svmt";
    auto start_time = std::chrono::high_resolution_clock::now();
    
    //model_runner::run_model(model_runner::model_name::PAR_LINEAR_MERGER, executor, 
    //    core_count, n_args, false, model_runner::clustering_type::STANDARD);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << "Full Runtime: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms" << std::endl;
    std::cout << "ALL NODES ENDED" << std::endl;
    */
    // delete bench;
    //delete executor;
    std::string binary_path = "";
    /*auto val_output = std::ofstream(binary_path + "../../output/benchmark_results/validation_result5.txt");
    auto val_run = validation(&val_output, "", true); 
    val_run.run_validation("");*/
    
    //auto perf_output = std::ofstream(binary_path + "../../output/benchmark_results/benchmark_result.txt");
    //auto perf_test = performance_test(&perf_output, false);
    //perf_test.run_all_benchmarks("");
}
