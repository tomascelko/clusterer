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
using standard_clustering_type = pixel_list_clusterer<cluster>;
using parallel_clusterer_type = parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>>;

void run_architecture(benchmarker * bench, const std::string & arch_name, uint32_t clustering_cores)
{
    std::string base_arch = /*"co1p1,*/    "rr1bm1,bm1s1," ;
    auto default_split_descr = new temporal_clustering_descriptor<mm_hit>(clustering_cores);
    auto default_merge_descr = new temporal_clustering_descriptor<mm_hit>(clustering_cores);
    auto multi_merge_descr_1 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 1);
    auto multi_merge_descr_2 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 2,  false);
    auto hit_sorter_split_descr = new temporal_hit_split_descriptor<mm_hit>(clustering_cores);
    auto trivial_sorter_merge_descr = new trivial_merge_descriptor<mm_hit>();
    auto trivial_merger_split_descr = new trivial_split_descriptor<cluster<mm_hit>>();
    uint32_t TILE_SIZE = 4;
    uint32_t TIME_WINDOW_SIZE = 50000000;
    if(arch_name == "s")
         bench->run_whole_benchmark<standard_clustering_type>(architecture_type(base_arch + "s1sc1,sc1co1"));
    else if (arch_name == "e")
        bench->run_whole_benchmark<energy_filtering_clusterer<mm_hit>>(architecture_type(base_arch + "s1ec1"));
    else if (arch_name == "p")
        bench->run_whole_benchmark<parallel_clusterer_type>(
            architecture_type(base_arch + "s1ppc1,ppc1co1", std::map<std::string, abstract_node_descriptor*>{
                {"ppc1",new node_descriptor<cluster<mm_hit>, mm_hit>(default_merge_descr, default_split_descr, "packed_parallel_clusterer_descriptor")}
                })); 
    else if(arch_name == "pmm")
        bench->run_whole_benchmark<standard_clustering_type>(
            architecture_type(base_arch + "s1sc1,s1sc2,s1sc3,s1sc4,sc1m1,sc2m1,sc3m2,sc4m2,m1co1,m2co1,m3co1,m2m3,m1m3", std::map<std::string, abstract_node_descriptor*>{
                {"s1", new node_descriptor<mm_hit, mm_hit>(trivial_sorter_merge_descr, hit_sorter_split_descr, "sorter_descriptor")},
                {"m1", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(multi_merge_descr_1, multi_merge_descr_1, "merger1_desc")},
                {"m2", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(multi_merge_descr_1, multi_merge_descr_1, "merger2_descr")},
                {"m3", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(multi_merge_descr_2, multi_merge_descr_2, "merger3_descr")}
                }));
    else if (arch_name == "pm")
        bench->run_whole_benchmark<standard_clustering_type>(
            architecture_type(base_arch + "s1sc1,s1sc2,s1sc3,s1sc4,sc1m1,sc2m1,sc3m1,sc4m1,m1co1", std::map<std::string, abstract_node_descriptor*>{
                    {"s1", new node_descriptor<mm_hit, mm_hit>(trivial_sorter_merge_descr, hit_sorter_split_descr, "sorter_descriptor")},
                    {"m1", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(default_merge_descr, trivial_merger_split_descr, "merger_desc")}
                    }));
    else if (arch_name == "tr")
        bench->run_whole_benchmark<trigger_clusterer<mm_hit, standard_clustering_type, frequency_diff_trigger<mm_hit>>>(
            architecture_type(base_arch + "s1trc1,trc1co1"));
    else if (arch_name == "ti") //TODO try paralel clustering with tiles
        bench->run_whole_benchmark<pixel_list_clusterer<cluster>>(
            architecture_type(base_arch + "s1tic1,tic1co1"), TILE_SIZE);
    else if (arch_name == "bb")
        bench->run_whole_benchmark<halo_buffer_clusterer<mm_hit, standard_clustering_type>>(
            architecture_type(base_arch + "s1bbc1,bbc1co1"));
    else if (arch_name == "trbb")
    bench->run_whole_benchmark<trigger_clusterer<mm_hit,
        halo_buffer_clusterer<mm_hit, standard_clustering_type>, frequency_diff_trigger<mm_hit>>>(
            architecture_type(base_arch + "s1trc1,trc1co1"), TIME_WINDOW_SIZE);



}
int main(int argc, char** argv)
{
    const uint32_t TILE_SIZE = 2;
    const uint32_t TIME_WINDOW_SIZE = 50000000; //50ms
    std::vector<std::string> args (argv + 1, argc + argv);
    
    args = {"/home/tomas/MFF/DT/clusterer/test_data/large/", "/home/tomas/MFF/DT/clusterer/test_data/calib/", "pmm", "4"};
    std::string bench_folder = args[0];
    std::string calib_folder = args[1]; 
    std::string arch_type = args[2];
    uint16_t clustering_cores = std::stoi(args[3]);
    //temporal_clustering_descriptor<mm_hit>* descriptor_ = new temporal_clustering_descriptor<mm_hit>();
    benchmarker* bench = new benchmarker(bench_folder, calib_folder);
    
    run_architecture(bench, arch_type, clustering_cores);
    std::cout << "ALL ENDED" << std::endl;
    delete bench;
    //TODO add virtual destructors
}


