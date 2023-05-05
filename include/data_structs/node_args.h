#include <map>
#include <string>
#pragma once
class node_args
{

    using node_args_type = std::map<std::string, std::string>;
    std::map<std::string, node_args_type> args_data_;
    public:
    double get_double_arg(const std::string & node_name,  const std::string & arg_name) const
    {
        return std::stod(args_data_.at(node_name).at(arg_name));
    }
    double get_int_arg(const std::string & node_name,  const std::string & arg_name) const
    {
        return std::stoi(args_data_.at(node_name).at(arg_name));
    }
    const std::string & get_str_arg(const std::string & node_name,  const std::string & arg_name) const
    {
        return args_data_.at(node_name).at(arg_name);
    }
    const std::map<std::string, std::string> & at(const std::string & node_name) const
    {
        return args_data_.at(node_name);
    }
    std::map<std::string, std::string> & operator [](const std::string & node_name)
    {
        return args_data_[node_name];
    }
    void load_from_file(const std::string & filename)
    {
        //TODO - not implemented
    }

    node_args()
    {
        args_data_ = {
        {"clusterer",
            node_args_type({
                {"tile_size", "1"},
                {"max_dt", "300"}
            })
        },
        {"trigger", node_args_type(
            {
                {"window_size", "200000000"},
                {"diff_window_size", "0"},
                {"trigger_time", "500000000"},
                {"trigger_file", "example_trigger.nnt"}
            })
        },
        {"hallo_bb_clusterer", node_args_type(
            {
                {"window_size", "100000"}
            })

        },
        {"window_computer", node_args_type(
            {
                {"window_size", "200000000"},
                {"diff_window_size", "1000000000" }
            })    
        }
        };
    }
};