#include "../include/data_structs/burda_hit.h"
#include "../include/data_flow/dataflow_controller.h"
#include "../include/data_nodes/nodes_package.h"
#include "../include/data_structs/cluster.h"
#include "../include/mm_stream.h"
#include "../include/utils.h"
#include "../include/benchmark/i_time_measurable.h"
#include "../include/benchmark/benchmarker.h"
int main(int argc, char** argv)
{
    std::vector<std::string> args (argv + 1, argc + argv);
    std::string str = double_to_str(0.789);

    args = {"/home/tomas/MFF/DT/clusterer/test_data/medium/", "/home/tomas/MFF/DT/clusterer/test_data/calib/"};
    std::string bench_folder = args[0];
    std::string calib_folder = args[1]; 
    benchmarker* bench = new benchmarker(bench_folder, calib_folder);
    bench->run_whole_benchmark();
    //controller->start_all();
    std::cout << "ALL ENDED" << std::endl;
    delete bench;
    //TODO add virtual destructors
}