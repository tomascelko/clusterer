#include "../include/data_structs/burda_hit.h"
#include "../include/data_flow/dataflow_controller.h"
#include "../include/data_nodes/nodes_package.h"
#include "../include/data_structs/cluster.h"
#include "../include/mm_stream.h"
#include "../include/utils.h"
#include "../include/benchmark/i_time_measurable.h"
#include "../include/benchmark/benchmarker.h"
#include "../include/execution/executor.h"
#include "../include/execution/model_selector.h"

#include <iostream>
#include <fstream>
using standard_clustering_type = pixel_list_clusterer<cluster>;
using parallel_clusterer_type = parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>>;

int main(int argc, char** argv)
{
    const uint32_t TILE_SIZE = 2;
    const uint32_t TIME_WINDOW_SIZE = 50000000; //50ms
    std::vector<std::string> args (argv + 1, argc + argv);
    
    args = {"/home/tomas/MFF/DT/clusterer/test_data/small/", "/home/tomas/MFF/DT/clusterer/test_data/calib/", "pmm", "4"};
    std::string data_folder = args[0];
    std::vector<std::string> cluster_comparison_datasets = {"/home/tomas/MFF/DT/clusterer/test_data/validation/small/\
new28-12-2022-19-28-17.ini",
        "/home/tomas/MFF/DT/clusterer/test_data/validation/small/\
Clustered - pions_180GeV_bias200V_60deg_alignment_test_F4-W00076_1.txt - 19-47 28-12-2022.ini"
    };
    std::string calib_folder = args[1]; 
    std::string arch_type = args[2];
    uint16_t clustering_cores = std::stoi(args[3]);

    std::string output_folder = "/home/tomas/MFF/DT/clusterer/output/";
    std::string data_file = "/home/tomas/MFF/DT/clusterer/test_data/small/pions_180GeV_bias200V_60deg_alignment_test_F4-W00076_1.txt";
    
    benchmarker* bench = new benchmarker(data_folder, calib_folder, "/home/tomas/MFF/DT/clusterer/output/");
    //model_executor * comp_executor = new model_executor(cluster_comparison_datasets, 
    
    //std::vector<std::string>(), "/home/tomas/MFF/DT/clusterer/output/");
    model_executor * executor = new model_executor(std::vector<std::string> {data_file}, 
    calib_folder, output_folder);
    
    model_selector::print = true;
    model_selector::select_model(model_selector::model_name::WINDOW_COMPUTER, executor, 4, "", 100000000.);
    std::cout << "ALL ENDED" << std::endl;
    delete bench;
    delete executor;
    //TODO add virtual destructors
}


