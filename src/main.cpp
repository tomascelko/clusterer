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


int main(int argc, char **argv)
{
    //auto controller = udp_controller("192.168.1.153");
    
    //return 0;
    std::cout << "Starting the run..." << std::endl;
    std::cout << "Parsing the arguments..." << std::endl;
    const uint32_t TILE_SIZE = 2;
    const uint32_t TIME_WINDOW_SIZE = 100000000; // 50ms
    std::vector<std::string> args(argv, argc + argv);
    
    uint32_t core_count = 6; //args.size() > 1 ? std::stoi(args[1]) : 4;
    args = {"./clusterer", "--debug", "--n_workers", "6", "--merge_type", "multioutput", "--args" , "../../../clusterer_data/parameters/node_params.txt", "--calib", 
    "../../../clusterer_data/calib/K7/calib_coeffs/", "../../../clusterer_data/lead/deg0_0.txt"};

    argument_parser parser = argument_parser(args[0]);
    parser.try_parse_and_run(std::vector<std::string>(args.begin() + 1, args.end()));
    return 0;
    
    std::string output_folder = "../../output/";
    std::vector<std::string> data_files{
      "pion/pions_180GeV_deg_0.txt",
      "pion/pions_180GeV_deg_30.txt",
        "lead/deg0_0.txt",
        "lead/deg90_2.txt",
        "neutron/beam_T0trigger_0deg_150V_10s_0.txt",
        "neutron/neutrons_60deg_200V_F4-W00076_1.txt",
    };
    
    std::string binary_path = "";
    auto val_output = std::ofstream(binary_path + "../../output/benchmark_results_laptop/validation_result.txt");
    auto val_run = validation(&val_output, "", true); 
    val_run.run_validation("");
    
}
