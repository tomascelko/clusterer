#include "../include/data_structs/burda_hit.h"
#include "../include/data_flow/dataflow_controller.h"
#include "../include/data_nodes/nodes_package.h"
#include "../include/data_structs/cluster.h"
#include "../include/mm_stream.h"
#include "../include/utils.h"
#include "../include/benchmark/i_time_measurable.h"
#include "../include/benchmark/benchmarker.h"
#include <iostream>
#include <fstream>
using parallel_clusterer_type = parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>>;
int main(int argc, char** argv)
{

    std::vector<std::string> args (argv + 1, argc + argv);
    
    args = {"/home/tomas/MFF/DT/clusterer/test_data/large/", "/home/tomas/MFF/DT/clusterer/test_data/calib/", "pp", "3"};
    std::string bench_folder = args[0];
    std::string calib_folder = args[1]; 
    std::string clustering_type = args[2];
    uint16_t clustering_cores = std::stoi(args[3]);
    //temporal_clustering_descriptor<mm_hit>* descriptor_ = new temporal_clustering_descriptor<mm_hit>();
    benchmarker* bench = new benchmarker(bench_folder, calib_folder);
    if(clustering_type == "s")
         bench->run_whole_benchmark<pixel_list_clusterer<cluster>>("simple_clustering");
    else if (clustering_type == "e")
        bench->run_whole_benchmark<energy_filtering_clusterer<mm_hit>>("energy_filtering_clustering");
    else if (clustering_type == "p")
        bench->run_whole_benchmark<parallel_clusterer_type>(
            "simple_parallel", new temporal_clustering_descriptor<mm_hit>(clustering_cores)); 
    else if (clustering_type == "pp")
        bench->run_whole_benchmark<pixel_list_clusterer<cluster>>(
            new temporal_clustering_descriptor<mm_hit>(clustering_cores), "parallel_heap_clustering"); 
    else if (clustering_type == "t")
        bench->run_whole_benchmark<trigger_clusterer<mm_hit, frequency_diff_trigger<mm_hit>>>("triggering_clustering");
    //controller->start_all();
    std::cout << "ALL ENDED" << std::endl;
    delete bench;
    //TODO add virtual destructors
}