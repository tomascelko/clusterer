#include "../utils.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "../data_flow/dataflow_package.h"
#include "i_time_measurable.h"
#include "measuring_clock.h"
#include "../data_nodes/nodes_package.h"
#include "../mm_stream.h"
#include "../data_structs/node_args.h"
#include <tuple>
#pragma once
struct node
{
    std::string type;
    uint32_t id;
    node(const std::string & type, uint32_t id) :
    type(type),
    id(id){}
    bool operator == (const node & other)
    {
        return type == other.type && id == other.id;
    }
};
struct edge
{
    node from;
    node to;
    edge(node from, node to) : 
    from(from),
    to(to){}
};
class architecture_type
{
    
    std::vector<edge> edges_;
    std::vector<node> nodes_;
    std::map<std::string, abstract_node_descriptor*> node_descriptors_; 
    uint32_t tile_size_ = 1;
    double window_size_ = 50000000.;
    bool is_letter(char ch)
    {
        return ch >= 'a' && ch <= 'z';
    }
    bool is_digit(char ch)
    {
        return ch >= '0' && ch <= '9';
    }
    uint32_t read_numbers(const std::string & str, uint32_t & index)
    {
        std::string num;
        while (index < str.size() && is_digit(str[index]))
        {
            
            num.push_back(str[index]);
            ++index;
        } 
        while(index < str.size() && !is_letter(str[index]) && !is_digit(str[index]))
        {
            ++index; //we are skipping non alphanumerical characters
        }
        return std::stoi(num);

    }
    std::string read_letters(const std::string & str, uint32_t & index)
    {
        std::string word;
        while (index < str.size() && is_letter(str[index]))
        {
            word.push_back(str[index]);
            ++index;
        } 
        return word;

    }
    void init_arch(const std::string & arch_string)
    {
        uint32_t i = 0;
        while (i < arch_string.size())
        {
            std::string type = read_letters(arch_string, i);
            uint32_t id = read_numbers(arch_string, i);
            auto from_node = node(type, id);
            type = read_letters(arch_string, i);
            id = read_numbers(arch_string, i);
            auto to_node = node(type, id);
            edges_.push_back(edge(from_node, to_node));
            if(std::find(nodes_.begin(), nodes_.end(), from_node) == nodes_.end())
                nodes_.push_back(from_node);
            if(std::find(nodes_.begin(), nodes_.end(), to_node) == nodes_.end())
                nodes_.push_back(to_node);
        }
    }
    public:
    architecture_type(const std::string & arch_string, uint32_t tile_size = 1, double window_size = 50000000) :
    window_size_(window_size),
    tile_size_(tile_size)
    {
        init_arch(arch_string);
    }

    architecture_type(const std::string & arch_string, std::map<std::string, abstract_node_descriptor*> node_descriptors, uint32_t tile_size = 1, double window_size = 50000000) :
    node_descriptors_(node_descriptors),
    window_size_(window_size),
    tile_size_(tile_size)
    {
        init_arch(arch_string);
    }
    std::map<std::string, abstract_node_descriptor*>& node_descriptors()
    {
        return node_descriptors_;
    } 
    
    std::vector<node> & nodes()
    {
        return nodes_;
    }
    std::vector<edge> & edges()
    {
        return edges_;
    }
    uint32_t tile_size()
    {
        return tile_size_;
    }
    double window_size()
    {
        return window_size_;
    }



};

class model_factory
{
    std::string data_file;
    std::string calib_file;
    using standard_clustering_type = pixel_list_clusterer<cluster>;
    using parallel_clusterer_type = parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>>;
    mm_write_stream * print_stream;
    using filenames_type = const std::vector<std::string>; 
    filenames_type::const_iterator data_file_it;
    filenames_type::const_iterator calib_file_it;
    static constexpr double FREQUENCY_MULTIPLIER_ = 1; 
    std::ofstream * window_print_stream;
    
    template <typename arch_type>
    i_data_node* create_node(node node, arch_type arch, const node_args & args)
    {
         
        if(node.type == "r")
            return new data_reader<burda_hit, std::ifstream>(*data_file_it, 2 << 10);
        else if(node.type == "rr")
            return new repeating_data_reader<burda_hit, std::ifstream>{*data_file_it, 2 << 21, FREQUENCY_MULTIPLIER_};  
        else if(node.type == "rc")
            return new data_reader<cluster<mm_hit>, mm_read_stream>(*data_file_it, 2 << 10);
        else if(node.type == "bm")
        {
            if (arch.node_descriptors().find(node.type + std::to_string(node.id)) != arch.node_descriptors().end())
                return new burda_to_mm_hit_adapter<mm_hit>(
                    dynamic_cast<node_descriptor<burda_hit, mm_hit>*>(arch.node_descriptors()["bm" + std::to_string(node.id)]), 
                        calibration(*calib_file_it, current_chip::chip_type::size()));
            else
                    return new burda_to_mm_hit_adapter<mm_hit>(calibration(*calib_file_it, current_chip::chip_type::size()));
        }
        else if(node.type == "s")
        {
            if (arch.node_descriptors().find(node.type + std::to_string(node.id)) != arch.node_descriptors().end())
                return new hit_sorter<mm_hit>(
                    dynamic_cast<node_descriptor<mm_hit, mm_hit>*>(arch.node_descriptors()["s" + std::to_string(node.id)]));
            else
                return new hit_sorter<mm_hit>();
        } 
        else if(node.type == "p")
            return new data_printer<cluster<mm_hit>, mm_write_stream>(print_stream);
        else if(node.type == "wp")
            return new data_printer<default_window_feature_vector<mm_hit>, std::ofstream>(window_print_stream);
        else if(node.type == "m")
            return new cluster_merging<mm_hit>(
                dynamic_cast<node_descriptor<cluster<mm_hit>, cluster<mm_hit>>*>(arch.node_descriptors()["m" + std::to_string(node.id)]));
        else if(node.type == "sc")
        {
            if (arch.node_descriptors().find(node.type + std::to_string(node.id)) != arch.node_descriptors().end())
                return new standard_clustering_type(
                    dynamic_cast<node_descriptor<mm_hit, cluster<mm_hit>>*>(arch.node_descriptors()["sc" + std::to_string(node.id)]), args);
                return new standard_clustering_type(args);
        }
        else if(node.type == "ec")
            return new energy_filtering_clusterer<mm_hit>();
        else if(node.type == "ppc")
            return new parallel_clusterer_type(
                dynamic_cast<node_descriptor<cluster<mm_hit>, mm_hit>*>(arch.node_descriptors()["ppc" + std::to_string(node.id)]), args);
        //else if(node.type == "trc")
        //    return new trigger_clusterer<mm_hit, standard_clustering_type, frequency_diff_trigger<mm_hit>>();
        else if(node.type == "tic")
            return new standard_clustering_type(args);
        else if(node.type == "bbc")
            return new halo_buffer_clusterer<mm_hit, standard_clustering_type>(args);
        //else if(node.type == "trbbc")
        //    return new trigger_clusterer<mm_hit, halo_buffer_clusterer<mm_hit, standard_clustering_type>, frequency_diff_trigger<mm_hit>>(
        //        args);
        else if(node.type == "tr")
            return new trigger_node<mm_hit, interval_trigger<mm_hit>>(args);
        else if(node.type == "co")
            return new cluster_sorting_combiner<mm_hit>();
        else if (node.type == "cv")
            return new clustering_validator<mm_hit>(std::cout);
        else if (node.type == "wfc")
            return new window_feature_computer<default_window_feature_vector<mm_hit>, mm_hit>(args);
        else 
            throw std::invalid_argument("node of given type was not implemented yet");

        throw std::invalid_argument("node of given type was not recognized");
        
    };
    void set_output_streams(const std::string & node_type, const std::string & output_dir, 
        const std::string & time_str, const std::string & id)
    {
        if(output_dir.size() > 0 && ((output_dir[output_dir.size() - 1] == '/') || (output_dir[output_dir.size() - 1] == '\\')))
        {
            if(node_type[0] == 'p')
                this->print_stream = new mm_write_stream(output_dir + "clustered_" + id + "_" + time_str);
            else
                this->window_print_stream = new std::ofstream(output_dir + "window_features_" + id + "_" + time_str);
        }
        else
        {
            if(node_type[0] == 'p')
                this->print_stream = new mm_write_stream(output_dir + id);
            else
                this->window_print_stream = new std::ofstream(output_dir);    
        }
    }
    public:
    
    void create_model(dataflow_controller * controller, architecture_type arch,
      const std::string& data_file, const std::string& calib_file, 
      const std::string output_dir, const node_args & args)
    {
        create_model(controller, arch, std::vector<std::string>{data_file}, 
        std::vector<std::string>{calib_file}, output_dir, args);
    }
    //TODO FINISH METHOD THAT GETS INPUT AS VECTORS
   
    void create_model(dataflow_controller * controller, architecture_type arch,
      const std::vector<std::string> & data_files, const std::vector<std::string> & calib_files,  
       const std::string & output_dir, const node_args & args)
    {
        auto t = std::time(nullptr);
        auto time = *std::localtime(&t);
        std::stringstream time_ss;
        time_ss << std::put_time(&time, "%d-%m-%Y_%H_%M_%S");
        
        std::vector<i_data_node*> data_nodes;
        data_file_it = data_files.cbegin();
        calib_file_it = calib_files.cbegin();
        auto output_node_index = 0;
        for (auto node : arch.nodes())
        {
            std::cout << "creating node " << node.type << std::endl;
            if(node.type == "p" || node.type == "wp")
            {
                set_output_streams(node.type, output_dir, time_ss.str(), std::to_string(output_node_index));
                output_node_index ++;
            }
            data_nodes.emplace_back(create_node(node, arch, args));
            
            
            controller->add_node(data_nodes.back());
            if(node.type[0] == 'r')
                ++data_file_it;
            if(node.type == "bm")
                ++calib_file_it;
            
        }   
        for(auto edge : arch.edges())
        {
            auto from_index = std::find(arch.nodes().begin(), arch.nodes().end(), edge.from) - arch.nodes().begin();
            auto to_index = std::find(arch.nodes().begin(), arch.nodes().end(), edge.to) - arch.nodes().begin();
            std::cout << "edges " << from_index << " " << to_index << std::endl;
            controller->connect_nodes(data_nodes[from_index], data_nodes[to_index]);
            
        }
        std::cout << "edges connected" << std::endl;
    }
};