#include <map>
#include <string>
#pragma once
// the class stores arguments for each node in the computational architecture
// by default it support four data types of arguments (int, double, bool and
// string)
class node_args {

  using node_args_type = std::map<std::string, std::string>;

  std::map<std::string, node_args_type> args_data_;
  void check_existence_file(const std::string &node_name,
                            const std::string &arg_name) const {
    if (args_data_.count(node_name) == 0) {
      throw std::invalid_argument("Argument '" + arg_name + "' for node '" +
                                  node_name + "' not foound");
    }
    if (args_data_.at(node_name).count(arg_name) == 0) {
      throw std::invalid_argument("Argument '" + arg_name + "' for node '" +
                                  node_name + "' not foound");
    }
  }

public:
  template <typename arg_type>
  arg_type get_arg(const std::string &node_name,
                   const std::string &arg_name) const {
    // By default, if no template parameter specified we return string
    check_existence_file(node_name, arg_name);
    return args_data_.at(node_name).at(arg_name);
  }
  // safe accessor, throws exception on not_found
  const std::map<std::string, std::string> &
  at(const std::string &node_name) const {

    return args_data_.at(node_name);
  }
  // can be used for modification from outside
  std::map<std::string, std::string> &operator[](const std::string &node_name) {
    return args_data_[node_name];
  }
  // default values for parameters
  node_args() {
    args_data_ = {
        {"reader", node_args_type({{"split", "true"},
                                   {"repetition_count", "300"},
                                   {"repetition_size", "1000000"},
                                   {"freq_multiplier", "1"},
                                   {"sleep_duration_full_memory", "100"}})},
        {"calibrator", node_args_type({{"split", "false"}})},
        {"sorter", node_args_type({{"split", "false"}})},
        {"clusterer",
         node_args_type(
             {{"tile_size", "1"}, {"max_dt", "200"}, {"type", "standard"}})},
        {"trigger",
         node_args_type(
             {{"window_size", "200000000"},
              {"diff_window_size", "1000000000"},
              {"trigger_time", "500000000"},
              {"trigger_file", "/home/tomas/MFF/DT/window_trigger_creator/"
                               "output/trained_beam_change.nnt"},
              {"split", "true"},
              {"use_trigger", "false"}

             })},
        {"halo_bb_clusterer", node_args_type({{"window_size", "1000"}})

        },
        {"window_computer", node_args_type({{"window_size", "200000000"},
                                            {"diff_window_size", "0"}})}

    };
  }
};

// parse integral argument from string
template <>
int node_args::get_arg<int>(const std::string &node_name,
                            const std::string &arg_name) const {

  return std::stoi(args_data_.at(node_name).at(arg_name));
};
// parse integral argument from double
template <>
double node_args::get_arg<double>(const std::string &node_name,
                                  const std::string &arg_name) const {
  return std::stod(args_data_.at(node_name).at(arg_name));
};
// parse string argument
template <>
std::string node_args::get_arg<std::string>(const std::string &node_name,
                                            const std::string &arg_name) const {
  check_existence_file(node_name, arg_name);
  return args_data_.at(node_name).at(arg_name);
};
// parse boolean argument
template <>
bool node_args::get_arg<bool>(const std::string &node_name,
                              const std::string &arg_name) const {
  return args_data_.at(node_name).at(arg_name) == "true" ? true : false;
};