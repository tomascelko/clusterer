
// TODO: arch will store only single abstract pipe descriptor
// update factory and model runner accordingly (SPLITTER ONLY)
// merger will just recast the same object to mergingdescriptor
// SET deafult template parameters of nodes to expected split decriptor type
// then no type specification is needed
#pragma once
#include "../data_flow/dataflow_package.h"
#include "../data_nodes/analysis/cluster_property_computer.h"
#include "../data_nodes/nodes_package.h"
#include "../data_structs/node_args.h"
#include "../mm_stream.h"
#include "../utils.h"
#include <functional>
#include <map>
#include <string>
#include <vector>
// #include "model_runner.h"
#include <tuple>
// corresponds to a single node in a computational graph
struct node {
  std::string type;
  uint32_t id;
  node(const std::string &type, uint32_t id) : type(type), id(id) {}
  bool operator==(const node &other) {
    return type == other.type && id == other.id;
  }
};

// corresponds to a single edge in a computational graph
struct edge {
  node from;
  node to;
  edge(node from, node to) : from(from), to(to) {}
};
// an auxiliary structure which helps in creation of the dataflow grap
// it is initialized from a string (list of edges) and a list of node
// descriptors
class architecture_type {

  std::vector<edge> edges_;
  std::vector<node> nodes_;
  std::map<std::string, abstract_pipe_descriptor *> node_descriptors_;
  uint32_t tile_size_ = 1;
  double window_size_ = 50000000.;
  bool is_letter(char ch) { return ch >= 'a' && ch <= 'z'; }
  bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }
  uint32_t read_numbers(const std::string &str, uint32_t &index) {
    std::string num;
    while (index < str.size() && is_digit(str[index])) {

      num.push_back(str[index]);
      ++index;
    }
    while (index < str.size() && !is_letter(str[index]) &&
           !is_digit(str[index])) {
      ++index; // we are skipping non alphanumerical characters
    }
    return std::stoi(num);
  }
  std::string read_letters(const std::string &str, uint32_t &index) {
    std::string word;
    while (index < str.size() && is_letter(str[index])) {
      word.push_back(str[index]);
      ++index;
    }
    return word;
  }
  void init_arch(const std::string &arch_string) {
    uint32_t i = 0;
    while (i < arch_string.size()) {
      std::string type = read_letters(arch_string, i);
      uint32_t id = read_numbers(arch_string, i);
      auto from_node = node(type, id);
      type = read_letters(arch_string, i);
      id = read_numbers(arch_string, i);
      auto to_node = node(type, id);
      edges_.push_back(edge(from_node, to_node));
      if (std::find(nodes_.begin(), nodes_.end(), from_node) == nodes_.end())
        nodes_.push_back(from_node);
      if (std::find(nodes_.begin(), nodes_.end(), to_node) == nodes_.end())
        nodes_.push_back(to_node);
    }
  }

public:
  architecture_type(const std::string &arch_string, uint32_t tile_size = 1,
                    double window_size = 50000000)
      : window_size_(window_size), tile_size_(tile_size) {
    init_arch(arch_string);
  }

  architecture_type(
      const std::string &arch_string,
      std::map<std::string, abstract_pipe_descriptor *> node_descriptors,
      uint32_t tile_size = 1, double window_size = 50000000)
      : node_descriptors_(node_descriptors), window_size_(window_size),
        tile_size_(tile_size) {
    init_arch(arch_string);
  }
  std::map<std::string, abstract_pipe_descriptor *> &node_descriptors() {
    return node_descriptors_;
  }

  std::vector<node> &nodes() { return nodes_; }
  std::vector<edge> &edges() { return edges_; }
  uint32_t tile_size() { return tile_size_; }
  double window_size() { return window_size_; }
};
// given controller and architecture_type
// creates the real execution nodes of the computational graph
class model_factory {
  static constexpr bool use_online_reading = true;
  static constexpr double FREQUENCY_MULTIPLIER_ = 1;
  std::string data_file;
  std::string calib_file;
  using standard_clustering_type = pixel_list_clusterer<cluster>;
  using parallel_clusterer_type =
      parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>>;
  mm_write_stream *print_stream;
  using filenames_type = const std::vector<std::string>;
  filenames_type::const_iterator data_file_it;
  filenames_type::const_iterator calib_file_it;
  std::vector<std::string> all_data_files;
  std::ofstream *window_print_stream;
  std::ostream *appending_output_stream;
  bool adapter_sorting = true;
  using burda_hit_split_descriptor = temporal_hit_split_descriptor<burda_hit>;
  using mm_hit_split_descriptor = temporal_hit_split_descriptor<mm_hit>;
  using cluster_two_splitting_descriptor =
      clustering_two_split_descriptor<cluster<mm_hit>>;
  using merge_descriptor = temporal_clustering_descriptor<mm_hit>;
  using standard_reader_type =
      std::conditional<use_online_reading, online_sync_data_reader<>,
                       data_reader<burda_hit, std::ifstream>>;
  // converts auxiliary node type to real i_data_node
  // the controller holds ownership of this node
  template <typename arch_type>
  i_data_node *create_node(node node, arch_type arch, const node_args &args) {

    if (node.type == "r") {
      if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
          arch.node_descriptors().end()) {
        // return new data_reader<burda_hit, std::ifstream>{
        return new standard_reader_type::type{
            dynamic_cast<burda_hit_split_descriptor *>(
                arch.node_descriptors()["r" + std::to_string(node.id)]),
            *data_file_it, args};
      } else {
        // return new data_reader<burda_hit, std::ifstream>{
        return new standard_reader_type::type{*data_file_it, args};
      }
    } else if (node.type == "rr") {
      if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
          arch.node_descriptors().end())
        return new repeating_data_reader<burda_hit, std::ifstream>{
            dynamic_cast<burda_hit_split_descriptor *>(
                arch.node_descriptors()["rr" + std::to_string(node.id)]),
            *data_file_it, args, calib_file};
      else
        return new repeating_data_reader<burda_hit, std::ifstream>{
            *data_file_it, args, calib_file};
    } else if (node.type == "rc") {
      return new data_reader<cluster<mm_hit>, mm_read_stream>(*data_file_it,
                                                              args);
    } else if (node.type == "bm") {
      if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
          arch.node_descriptors().end())
        return new burda_to_mm_hit_adapter<mm_hit>(
            dynamic_cast<mm_hit_split_descriptor *>(
                arch.node_descriptors()["bm" + std::to_string(node.id)]),
            calibration(*calib_file_it, current_chip::chip_type::size()),
            adapter_sorting); // only allow the sorting capability if no sorter
                              // exists
      else
        return new burda_to_mm_hit_adapter<mm_hit>(
            calibration(*calib_file_it, current_chip::chip_type::size()),
            adapter_sorting);
    } else if (node.type == "s") {
      if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
          arch.node_descriptors().end())
        return new hit_sorter<mm_hit>(dynamic_cast<mm_hit_split_descriptor *>(
            arch.node_descriptors()["s" + std::to_string(node.id)]));
      else
        return new hit_sorter<mm_hit>();
    } else if (node.type == "p")
      return new data_printer<cluster<mm_hit>, mm_write_stream>(print_stream);
    /*else if (node.type == "wp")
      return new data_printer<default_window_feature_vector<mm_hit>,
                              std::ofstream>(window_print_stream);
    */
    else if (node.type == "m")
      return new cluster_merging<mm_hit>(
          dynamic_cast<merge_descriptor *>(
              arch.node_descriptors()["m" + std::to_string(node.id)]),
          args);
    else if (node.type == "sc") {
      if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
          arch.node_descriptors().end())
        return new standard_clustering_type(
            dynamic_cast<cluster_two_splitting_descriptor *>(
                arch.node_descriptors()["sc" + std::to_string(node.id)]),
            args);
      return new standard_clustering_type(args);
    } /* else if (node.type == "ec")
       return new energy_filtering_clusterer<mm_hit>();
     else if (node.type == "ppc")
       return new parallel_clusterer_type(
           dynamic_cast<node_descriptor<cluster<mm_hit>, mm_hit> *>(
               arch.node_descriptors()["ppc" + std::to_string(node.id)]),
           args);
     // else if(node.type == "trc")
     //     return new trigger_clusterer<mm_hit, standard_clustering_type,
     //     frequency_diff_trigger<mm_hit>>();
     else if (node.type == "tic")
       return new standard_clustering_type(args);
     else if (node.type == "bbc")
       return new halo_buffer_clusterer<mm_hit, standard_clustering_type>(args);
     else if (node.type == "tr") {
       if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
           arch.node_descriptors().end())
         return new trigger_node<mm_hit>(
             dynamic_cast<node_descriptor<mm_hit, mm_hit> *>(
                 arch.node_descriptors()["tr" + std::to_string(node.id)]),
             args);
       else
         return new trigger_node<mm_hit>(args);
     }*/
    else if (node.type == "co")
      return new cluster_sorting_combiner<cluster<mm_hit>>();
    else if (node.type == "cow")
      return new cluster_sorting_combiner<
          default_window_feature_vector<mm_hit>>();
    /*else if (node.type == "cv")
      return new clustering_validator<mm_hit>();
    else if (node.type == "wfc")
      return new window_feature_computer<default_window_feature_vector<mm_hit>,
     mm_hit > (args);*/
    else if (node.type == "tc") {
      if (arch.node_descriptors().find(node.type + std::to_string(node.id)) !=
          arch.node_descriptors().end())
        return new temporal_clusterer<cluster>(
            dynamic_cast<cluster_two_splitting_descriptor *>(
                arch.node_descriptors()["tc" + std::to_string(node.id)]),
            args);
      else
        return new temporal_clusterer<cluster>(args);
    } /* else if (node.type == "cp")
       return new cluster_property_computer<cluster, mm_hit>();
     */
    else if (node.type == "cs")
      return new cluster_splitter(
          dynamic_cast<cluster_two_splitting_descriptor *>(
              arch.node_descriptors()["cs" + std::to_string(node.id)]));
    else
      throw std::invalid_argument("node of given type was not implemented yet");

    throw std::invalid_argument("node of given type was not recognized");
  };
  std::string set_output_streams(const std::string &node_type,
                                 const std::string &output_dir,
                                 const std::string &time_str,
                                 const std::string &id) {
    std::string file_name;
    std::string input_file_name = file_path(all_data_files[0]).last_folder();
    if (output_dir.size() > 0 &&
        ((output_dir[output_dir.size() - 1] == '/') ||
         (output_dir[output_dir.size() - 1] == '\\'))) {
      if (node_type[0] == 'p') {
        file_name =
            output_dir + "clustered_" + input_file_name + id + "_" + time_str;
      } else
        file_name = output_dir + "window_features_" + input_file_name + id +
                    "_" + time_str;
    } else {
      if (node_type[0] == 'p')
        file_name = output_dir + id;
      else
        file_name = output_dir;
    }

    if (node_type[0] == 'p') {
      this->print_stream = new mm_write_stream(file_name);
    } else {
      this->window_print_stream = new std::ofstream(file_name);
    }
    return file_name;
  }

public:
  std::vector<std::string>
  create_model(dataflow_controller *controller, architecture_type arch,
               const std::string &data_file, const std::string &calib_file,
               const std::string output_dir, const node_args &args) {
    return create_model(controller, arch, std::vector<std::string>{data_file},
                        std::vector<std::string>{calib_file}, output_dir, args);
  }
  std::vector<std::string>
  create_model(dataflow_controller *controller, architecture_type &arch,
               const std::vector<std::string> &data_files,
               const std::vector<std::string> &calib_files,
               const std::string &output_dir, const node_args &args) {
    all_data_files = data_files;
    auto t = std::time(nullptr);
    auto time = *std::localtime(&t);
    std::stringstream time_ss;
    time_ss << std::put_time(&time, "%d_%m_%Y_%H_%M_%S");
    std::vector<i_data_node *> data_nodes;
    data_file_it = data_files.cbegin();
    calib_file_it = calib_files.cbegin();
    if (calib_file_it != calib_files.cend())
      calib_file = *calib_file_it;
    else
      calib_file = "";
    auto output_node_index = 0;
    std::vector<std::string> output_names;
    for (auto node : arch.nodes()) {
      // std::cout << "creating node " << node.type << std::endl;
      if (node.type == "p" || node.type == "wp" || node.type == "cv") {
        output_names.push_back(
            set_output_streams(node.type, output_dir, time_ss.str(),
                               std::to_string(output_node_index)));
        output_node_index++;
      }
      data_nodes.emplace_back(create_node(node, arch, args));
      data_nodes.back()->set_id(node.id);

      controller->add_node(data_nodes.back());
      if (node.type[0] == 'r')
        ++data_file_it;
      if (node.type == "bm" && calib_files.size() > 1)
        ++calib_file_it;
    }
    for (auto edge : arch.edges()) {
      auto from_index =
          std::find(arch.nodes().begin(), arch.nodes().end(), edge.from) -
          arch.nodes().begin();
      auto to_index =
          std::find(arch.nodes().begin(), arch.nodes().end(), edge.to) -
          arch.nodes().begin();
      controller->connect_nodes(data_nodes[from_index], data_nodes[to_index]);
    }
    return output_names;
  }
};