#pragma once
#include <string>
#include "executor.h"

//based on the options specified by user, form a model recipe (string)
class model_runner
{
public:
    static bool print;
    static bool recurring;
    static bool cluster_properties;
    enum class model_name
    {
        SIMPLE_CLUSTERER,
        TRIGGER_SIMPLE_CLUSTERER,
        TRIG_CLUSTERER,
        PAR_WITH_SPLITTER,
        PAR_SIMPLE_MERGER,
        PAR_TREE_MERGER,
        PAR_LINEAR_MERGER,
        VALIDATION,
        WINDOW_COMPUTER,
        PAR_MULTIFILE_CLUSTERER,

        // to be deprecated:
        ENERGY_TRIG_CLUSTERER,
        TRIG_BB_CLUSTERER,
        TILE_CLUSTERER
    };
    enum class clustering_type
    {
        STANDARD,
        TILED,
        HALO_BB,
        TEMPORAL
    };

private:
    static constexpr const char *base_arch = "r1bm1,bm1s1";
    //convert clustering type to string
    static std::string get_clustering_str(clustering_type clustering)
    {
        switch (clustering)
        {
        case clustering_type::STANDARD:
            return "sc";
        case clustering_type::TEMPORAL:
            return "tc";
        case clustering_type::HALO_BB:
            return "bbc";
        case clustering_type::TILED:
            return "tic";
        default:
            throw std::invalid_argument("Invalid clustering type");
        }
    }
    //based on node args, return the node of split
    static std::string create_split_node(const node_args &args, const std::string &first_node)
    {

        if (args.get_arg<bool>("reader", "split") && !args.get_arg<bool>("trigger", "use_trigger"))
            return first_node;

        else if (args.get_arg<bool>("trigger", "use_trigger") &&
                 args.get_arg<bool>("trigger", "split"))
            return "tr";
        else if (args.get_arg<bool>("calibrator", "split"))
            return "bm";
        else if (args.get_arg<bool>("sorter", "split"))
            return "s";
        else if (!args.get_arg<bool>("trigger", "use_trigger"))
            return first_node;
        else
            return "tr";
    }
    //create the first part of computational graph recipe
    //from reader up to clusterer
    static std::string create_split_arch(const node_args &args, const std::string &first_node,
                                         const std::string &cluster_node, uint32_t core_count)
    {
        std::string arch = "";
        std::vector<std::string> nodes_to_connect{first_node, "bm", "s", cluster_node};
        if (args.get_arg<bool>("trigger", "use_trigger"))
            nodes_to_connect.insert(nodes_to_connect.begin() + 2, "tr");
        std::string split_node = create_split_node(args, first_node);
        bool splitted = false;
        for (uint32_t node_index = 0; node_index < nodes_to_connect.size() - 1; ++node_index)
        {
            auto node = nodes_to_connect[node_index];
            if (splitted || node == split_node)
            {
                for (uint16_t i = 1; i <= core_count; i++)
                {
                    std::string this_index = splitted ? std::to_string(i) : "1";
                    std::string next_index = std::to_string(i);
                    std::string next_node = nodes_to_connect[node_index + 1] + next_index;
                    if (node_index != 0 || i != 1)
                        arch += ",";
                    arch += node + this_index + next_node;
                }
                splitted = true;
            }
            else
            {
                arch += node + "1" + nodes_to_connect[node_index + 1] + "1";
            }
        }

        return arch;
    }
    //create desccriptors for data splitting for each node where non-trivial splitting ocurring 
    static std::map<std::string, abstract_node_descriptor *> create_split_descriptors(const node_args &args,
                                                                                      uint32_t core_count)
    {
        if ((args.get_arg<bool>("reader", "split") ||
             (!args.get_arg<bool>("reader", "split") && !args.get_arg<bool>("sorter", "split") &&
              !args.get_arg<bool>("calibrator", "split"))) &&
            !args.get_arg<bool>("trigger", "use_trigger"))
        {
            return std::map<std::string, abstract_node_descriptor *>{
                {"r1",
                 new node_descriptor<burda_hit, burda_hit>(
                     new trivial_merge_descriptor<burda_hit>(),
                     new temporal_hit_split_descriptor<burda_hit>(core_count), "reader descriptor")},
                {"rr1",
                 new node_descriptor<burda_hit, burda_hit>(
                     new trivial_merge_descriptor<burda_hit>(),
                     new temporal_hit_split_descriptor<burda_hit>(core_count), "recurring reader descriptor")}};
        }
        else if (args.get_arg<bool>("calibrator", "split"))
        {
            return std::map<std::string, abstract_node_descriptor *>{
                {"bm1",
                 new node_descriptor<burda_hit, mm_hit>(
                     new trivial_merge_descriptor<burda_hit>(),
                     new temporal_hit_split_descriptor<mm_hit>(core_count), "calibrator descriptor")}};
        }
        else if (args.get_arg<bool>("sorter", "split"))
        {
            return std::map<std::string, abstract_node_descriptor *>{
                {"s1",
                 new node_descriptor<mm_hit, mm_hit>(
                     new trivial_merge_descriptor<mm_hit>(),
                     new temporal_hit_split_descriptor<mm_hit>(core_count), "sorter descriptor")}};
        }
        else // (args.get_arg<bool>("trigger", "use_trigger"))
        {
            return std::map<std::string, abstract_node_descriptor *>{
                {"tr1",
                 new node_descriptor<mm_hit, mm_hit>(
                     new trivial_merge_descriptor<mm_hit>(),
                     new temporal_hit_split_descriptor<mm_hit>(core_count), "trigger descriptor")}};
        }
        throw std::invalid_argument("");
    }
    //creates the second part of the model graph
    //includes merging, and outputting
    static void add_merge_arch(model_name model_type, std::string &arch,
                               std::map<std::string, abstract_node_descriptor *> &descriptors,
                               const std::string &clustering_node, const std::string &output_node, uint32_t core_count)
    {

        if (model_type == model_name::PAR_SIMPLE_MERGER)
        {
            auto trivial_merger_split_descr = new trivial_split_descriptor<cluster<mm_hit>>();
            auto default_merge_descr = new temporal_clustering_descriptor<mm_hit>(core_count);
            for (uint32_t i = 1; i <= core_count; ++i)
            {
                arch += "," + clustering_node + std::to_string(i) + "m1";
            }
            arch += ",m1" + output_node + "1";
            descriptors.insert(std::make_pair("m1",
                                              new node_descriptor<cluster<mm_hit>, cluster<mm_hit>>(default_merge_descr,
                                                                                                    trivial_merger_split_descr, "merger_desc")));
        }
        else if (model_type == model_name::PAR_LINEAR_MERGER || model_type == model_name::PAR_MULTIFILE_CLUSTERER)
        {

            if (output_node == "p" && model_type == model_name::PAR_LINEAR_MERGER)
                arch += ",co1p1";
            for (uint16_t i = 1; i <= core_count; i++)
            {
                auto trivial_hit_merge_descr = new trivial_merge_descriptor<mm_hit>();

                auto two_clustering_split_descriptor = new clustering_two_split_descriptor<cluster<mm_hit>>();
                auto two_clustering_merge_descriptor = new temporal_clustering_descriptor<mm_hit>(2);
                auto trivial_merger_split_descr = new trivial_split_descriptor<cluster<mm_hit>>();
                std::string str_index = std::to_string(i);
                std::string sc = clustering_node + str_index;
                std::string merg_low = "m" + str_index;
                std::string merg_high = "m" + std::to_string(((i) % (core_count)) + 1); // modulo in 1 based indexing
                arch += "," + sc + merg_low;
                arch += "," + sc + merg_high;
                if (model_type == model_name::PAR_MULTIFILE_CLUSTERER)
                    arch += "," + merg_low + output_node + str_index;
                if (model_type == model_name::PAR_LINEAR_MERGER)
                {
                    arch += "," + merg_low + "co1";
                }
                descriptors.insert({clustering_node + str_index,
                                    new node_descriptor<mm_hit, cluster<mm_hit>>(trivial_hit_merge_descr,
                                                                                 two_clustering_split_descriptor,
                                                                                 "clustering_" + str_index + "_descriptor")});
                descriptors.insert({"m" + str_index,
                                    new node_descriptor<cluster<mm_hit>, cluster<mm_hit>>(two_clustering_merge_descriptor,
                                                                                          trivial_merger_split_descr,
                                                                                          "merger_" + str_index + "_descriptor")});
            }
        }
        else
            throw std::invalid_argument("Specified model does not support explicit paralelism");
    }
    //facilitates the construction of the whole architecture recipe for data-based split architectures
    static architecture_type build_parallel_arch(model_name model_type,
                                                 const std::string &first_node, uint16_t core_count,
                                                 const std::string &output_node, const node_args &args, const std::string &clusterer_node = "sc")
    {

        std::map<std::string, abstract_node_descriptor *> descriptors = create_split_descriptors(args, core_count);
        std::string arch = create_split_arch(args, first_node, clusterer_node, core_count);
        add_merge_arch(model_type, arch, descriptors, clusterer_node, output_node, core_count);
        return architecture_type(arch, descriptors);
    }

public:
    //the main method of the runner, based on the model type, run the approperiate 
    //model building method
    template <typename executor_type, typename... clustering_args>
    static std::vector<std::string> run_model(model_name model_type, executor_type *executor,
                                              uint16_t core_count, node_args &args, bool debug_mode = false, clustering_type clustering_alg_type = clustering_type::STANDARD)
    {
        //std::cout << "Building the architecture..." << std::endl;
        std::string validation_arch = "rc1cv1,rc2cv1";

        auto multi_merge_descr_11 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 1);
        auto multi_merge_descr_12 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 1);
        auto multi_split_descr_11 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 1);
        auto multi_split_descr_12 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 1);

        auto multi_merge_descr_2 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 2, false);
        auto multi_split_descr_2 = new temporal_merge_descriptor<cluster<mm_hit>>(2, 2, false);
        auto hit_sorter_split_descr = new temporal_hit_split_descriptor<mm_hit>(core_count);
        auto trivial_hit_merge_descr = new trivial_merge_descriptor<mm_hit>();
        std::string arch = base_arch;
        std::string output_node;
        std::string first_node = "r";
        print = false;
        bool trigger = args.get_arg<bool>("trigger", "use_trigger");
        std::string clustering_node = get_clustering_str(clustering_alg_type);
        if (trigger)
        {
            arch = "r1bm1,bm1tr1,tr1s1";
        }
        if (recurring)
        {
            arch = "r" + arch;
            first_node = "rr";
        }
        if (print)
            output_node = "p";
        else
            output_node = "co";
        if (cluster_properties)
            arch += "," + output_node + "1" + "cp1";
        std::string default_output_node = output_node + "1";

        std::string sub_arch;
        switch (model_type)
        {

        case model_name::SIMPLE_CLUSTERER:
            return executor->run(architecture_type(arch + ",s1" + clustering_node + "1," +
                                                   clustering_node + "1" + default_output_node),
                                 args, debug_mode);
            break;
        case model_name::ENERGY_TRIG_CLUSTERER:
            return executor->run(architecture_type(arch + ",s1ec1"), args);
            break;
        case model_name::PAR_TREE_MERGER:
            return executor->run(
                architecture_type(arch +
                                      ",s1sc1,s1sc2,s1sc3,s1sc4,sc1m1,sc2m1,sc3m2,sc4m2,m1" + default_output_node + ",m2" + default_output_node + ",m3" + default_output_node + ",m2m3,m1m3",
                                  std::map<std::string, abstract_node_descriptor *>{
                                      {"s1", new node_descriptor<mm_hit, mm_hit>(trivial_hit_merge_descr, hit_sorter_split_descr, "sorter_descriptor")},
                                      {"m1", new node_descriptor<cluster<mm_hit>, cluster<mm_hit>>(multi_merge_descr_11, multi_split_descr_11, "merger1_desc")},
                                      {"m2", new node_descriptor<cluster<mm_hit>, cluster<mm_hit>>(multi_merge_descr_12, multi_split_descr_12, "merger2_descr")},
                                      {"m3", new node_descriptor<cluster<mm_hit>, cluster<mm_hit>>(multi_merge_descr_2, multi_split_descr_2, "merger3_descr")}}),
                args, debug_mode);
            break;
        case model_name::PAR_SIMPLE_MERGER:
            return executor->run(
                build_parallel_arch(model_type, first_node, core_count, output_node, args, clustering_node), args,
                debug_mode);
            break;
        case model_name::TRIG_CLUSTERER:
            return executor->run(
                architecture_type(arch + ",s1tr1,tr1" + default_output_node), args, debug_mode);
            break;
        case model_name::TILE_CLUSTERER:
            return executor->run(
                architecture_type(arch + ",s1tic1,tic1" + default_output_node), args, debug_mode);
            break;
        case model_name::TRIG_BB_CLUSTERER:
            return executor->run(
                architecture_type(arch + ",s1trbbc1,trbbc1" + default_output_node), args, debug_mode);
            break;
        case model_name::PAR_LINEAR_MERGER:
        case model_name::PAR_MULTIFILE_CLUSTERER:
            return executor->run(
                build_parallel_arch(model_type, first_node, core_count, output_node, args, clustering_node), args,
                debug_mode);
            break;
        case model_name::VALIDATION:
            return executor->run(
                architecture_type(validation_arch), args, debug_mode);
            break;
        case model_name::WINDOW_COMPUTER:
            if (print)
                return executor->run(architecture_type(arch + ",s1wfc1,wfc1wp1"), args, debug_mode);
            else
                return executor->run(architecture_type(arch + ",s1wfc1,wfc1cow1"), args, debug_mode);
            break;
        case model_name::TRIGGER_SIMPLE_CLUSTERER:
            sub_arch = arch.substr(0, arch.find(','));
            return executor->run(architecture_type(sub_arch + ",bm1tr1,tr1s1,s1" + clustering_node + "1," + clustering_node + "1" + default_output_node), args, debug_mode);
            break;
        default:
            throw std::invalid_argument("Invalid model name passed");
        }
        throw std::invalid_argument("Invalid model name passed");
    }
};
bool model_runner::print = true;
bool model_runner::recurring = false;
bool model_runner::cluster_properties = false;
