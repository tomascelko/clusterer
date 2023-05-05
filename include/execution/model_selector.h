#pragma once
#include <string>
#include "executor.h"

class model_selector
{
    public:
    static bool print;
    static bool recurring;
    enum class model_name
    {
        SIMPLE_CLUSTERER,
        ENERGY_TRIG_CLUSTERER,
        TILE_CLUSTERER,
        TRIG_CLUSTERER,
        BB_CLUSTERER,
        TRIG_BB_CLUSTERER,
        PAR_WITH_SPLITTER,
        PAR_WITH_MERGER,
        PAR_WITH_TREE_MERGER,
        PAR_WITH_LINEAR_MERGER,
        VALIDATION,
        WINDOW_COMPUTER,
        FULLY_PAR_CLUSTERER,
        TRIGGER_SIMPLE_CLUSTERER
    };
    private:
    static constexpr const char * base_arch = "r1bm1,bm1s1" ;
    
    static architecture_type build_arch(std::string & arch, model_name model_type, uint16_t core_count, 
        const std::string& output_node)
    {
        auto trivial_hit_merge_descr = new trivial_merge_descriptor<mm_hit>();
        auto trivial_burda_hit_merge_descr = new trivial_merge_descriptor<burda_hit>();

        auto hit_sorter_split_descr = new temporal_hit_split_descriptor<mm_hit>(core_count);
        auto default_merge_descr = new temporal_clustering_descriptor<mm_hit>(core_count);
        auto two_clustering_split_descriptor = new clustering_two_split_descriptor<cluster<mm_hit>>();
        auto two_clustering_merge_descriptor =  new temporal_clustering_descriptor<mm_hit>(2);
        auto trivial_merger_split_descr = new trivial_split_descriptor<cluster<mm_hit>>();
        
        if(model_type == model_name::PAR_WITH_MERGER)
        {
            for(uint16_t i = 1; i <= core_count; i++)
            {
                std::string str_index = std::to_string(i);
                std::string sc = "sc" + str_index;
                arch += ",s1" + sc;
                arch += "," + sc + "m1";
            }
            arch += ",m1" + output_node + "1";
            return architecture_type(arch, 
            std::map<std::string, abstract_node_descriptor*>{
                    {"s1", new node_descriptor<mm_hit, mm_hit>(trivial_hit_merge_descr, 
                        hit_sorter_split_descr, "sorter_descriptor")},
                    {"m1", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(default_merge_descr, 
                        trivial_merger_split_descr, "merger_desc")}
                    });
        }
        else if (model_type == model_name::PAR_WITH_LINEAR_MERGER || model_type ==  model_name::FULLY_PAR_CLUSTERER)
        {
            
            std::map<std::string, abstract_node_descriptor*> descriptors{
                {"bm1", new node_descriptor<burda_hit, mm_hit>(trivial_burda_hit_merge_descr, 
                    hit_sorter_split_descr, "burda_to_mm_descriptor")}
            };
            if(output_node == "p" && model_type == model_name::PAR_WITH_LINEAR_MERGER)
                        arch += ",co1p1";
            for(uint16_t i = 1; i <= core_count; i++)
            {
                std::string str_index = std::to_string(i);
                std::string so = "s" + str_index;
                std::string sc = "sc" + str_index;
                std::string merg_low = "m" + str_index;
                    
                std::string merg_high = "m" + std::to_string(((i) % (core_count)) + 1); //modulo in 1 based indexing
                
                if(i > 1)
                    arch += ",bm1" + so;
                arch += "," + so + sc;
                arch += "," + sc + merg_low;
                arch += "," + sc + merg_high;
                if(model_type == model_name::FULLY_PAR_CLUSTERER)
                    arch += "," + merg_low + output_node + str_index;
                if(model_type == model_name::PAR_WITH_LINEAR_MERGER)
                {
                    arch += "," + merg_low + "co1";
                    
                }

                descriptors.insert({"sc" + str_index, 
                    new node_descriptor<mm_hit, cluster<mm_hit>>(trivial_hit_merge_descr, 
                        two_clustering_split_descriptor, 
                        "clustering_" + str_index + "_descriptor")});
                descriptors.insert({"m" + str_index, 
                    new node_descriptor<cluster<mm_hit>, cluster<mm_hit>>(two_clustering_merge_descriptor, 
                        trivial_merger_split_descr, 
                        "merger_" + str_index + "_descriptor")});

            }
            return architecture_type(arch, descriptors);
        } 
        throw std::invalid_argument("Specified model does not support explicit paralelism");
    }
    public:
    template <typename executor_type, typename ...clustering_args>
    static void select_model(model_name model_type , executor_type* executor, 
        uint16_t core_count, const std::string & output_folder, const node_args & args)
    {
        
        std::string validation_arch = "rc1cv1,rc2cv1";
        auto default_split_descr = new temporal_clustering_descriptor<mm_hit>(core_count);
        auto default_merge_descr = new temporal_clustering_descriptor<mm_hit>(core_count);
        auto multi_merge_descr_1 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 1);
        auto multi_merge_descr_2 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 2,  false);
        auto hit_sorter_split_descr = new temporal_hit_split_descriptor<mm_hit>(core_count);
        auto trivial_hit_merge_descr = new trivial_merge_descriptor<mm_hit>();
        auto trivial_merger_split_descr = new trivial_split_descriptor<cluster<mm_hit>>();
    std::string arch = base_arch;
    std::string output_node;
    if(recurring)
    {
        arch = "r" + arch;
        std::cout << "recurring" << std::endl;
    }
    if(print)
        output_node = "p";
    else
        output_node = "co";
    std::string default_output_node = output_node + "1";
    uint32_t TILE_SIZE = 4;
    uint32_t TIME_WINDOW_SIZE = 50000000;
    if(model_type == model_name::SIMPLE_CLUSTERER)
         executor->run(architecture_type(arch + ",s1sc1,sc1" + default_output_node), args);
    else if (model_type == model_name::ENERGY_TRIG_CLUSTERER)
        executor->run(architecture_type(arch + ",s1ec1"), args);
    else if (model_type == model_name::PAR_WITH_SPLITTER)
        executor->run(
            architecture_type(arch + ",s1ppc1,ppc1"+ default_output_node , std::map<std::string, abstract_node_descriptor*>{
                {"ppc1",new node_descriptor<cluster<mm_hit>, mm_hit>(default_merge_descr, default_split_descr, "packed_parallel_clusterer_descriptor")}
                }), args); 
    else if(model_type == model_name::PAR_WITH_TREE_MERGER)
        executor->run(
            architecture_type(arch + 
            ",s1sc1,s1sc2,s1sc3,s1sc4,sc1m1,sc2m1,sc3m2,sc4m2,m1" + default_output_node + ",m2" + default_output_node + ",m3" + default_output_node + ",m2m3,m1m3", 
            std::map<std::string, abstract_node_descriptor*>{
                {"s1", new node_descriptor<mm_hit, mm_hit>(trivial_hit_merge_descr, hit_sorter_split_descr, "sorter_descriptor")},
                {"m1", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(multi_merge_descr_1, multi_merge_descr_1, "merger1_desc")},
                {"m2", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(multi_merge_descr_1, multi_merge_descr_1, "merger2_descr")},
                {"m3", new node_descriptor<cluster<mm_hit>,cluster<mm_hit>>(multi_merge_descr_2, multi_merge_descr_2, "merger3_descr")}
                }), args);
    else if (model_type == model_name::PAR_WITH_MERGER)
        executor->run(
            build_arch(arch, model_type, core_count, output_node), args
            );
    else if (model_type == model_name::TRIG_CLUSTERER)
        executor->run(
            architecture_type(arch + ",s1tr1,tr1" + default_output_node), args);
    else if (model_type == model_name::TILE_CLUSTERER)
        executor->run(
            architecture_type(arch + ",s1tic1,tic1"+ default_output_node), args);
    else if (model_type == model_name::BB_CLUSTERER)
        executor->run(
            architecture_type(arch + ",s1bbc1,bbc1" + default_output_node), args);
    else if (model_type == model_name::TRIG_BB_CLUSTERER)
        executor->run(
            architecture_type(arch + ",s1trbbc1,trbbc1" + default_output_node), args); 
    else if (model_type == model_name::PAR_WITH_LINEAR_MERGER || model_type == model_name::FULLY_PAR_CLUSTERER)
        executor->run(
            build_arch(arch, model_type, core_count, output_node), args
        );
    else if (model_type == model_name::VALIDATION)
        executor->run(
            architecture_type(validation_arch), args);  
    else if (model_type == model_name::WINDOW_COMPUTER)
        executor->run(architecture_type(arch + ",s1wfc1,wfc1wp1"), args);
    else if(model_type == model_name::TRIGGER_SIMPLE_CLUSTERER)
    {
        auto sub_arch = arch.substr(0,arch.find(','));
        executor->run(architecture_type(sub_arch + ",bm1tr1,tr1s1,s1sc1,sc1" + default_output_node), args);
    }
    }
    
};
bool model_selector::print = true;
bool model_selector::recurring = false;
