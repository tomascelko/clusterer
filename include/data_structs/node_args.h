#include <map>
#include <string>
#pragma once
class node_args
{

    using node_args_type = std::map<std::string, std::string>;
    std::map<std::string, node_args_type> args_data_;
    public:
    template<typename arg_type>
    arg_type get_arg(const std::string & node_name,  const std::string & arg_name) const
    {
        //By default, if no template parameter specified we return string
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
        {"reader",
            node_args_type({
                {"split", "true"},
                {"repetition_count", "80"},
                {"repetition_size", "1000000"},
                {"freq_multiplier", "1"},
                {"sleep_duration_full_memory", "100"}
            })
        },
        {"calibrator",
            node_args_type({
                {"split", "false"}
            })
        },
        {"sorter",
            node_args_type({
                {"split", "false"}
            })
        },
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
                {"trigger_file", "/home/tomas/MFF/DT/window_trigger_creator/output/abcd.nnt"},
                {"data_file", ""}
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
                {"diff_window_size", "0" }
            })    
        }
        
        };
    }
};

    template<>
    int node_args::get_arg<int>(const std::string & node_name,  const std::string & arg_name) const
    {
        return std::stoi(args_data_.at(node_name).at(arg_name));
    };
    template<>
    double node_args::get_arg<double>(const std::string & node_name,  const std::string & arg_name) const
    {
        return std::stod(args_data_.at(node_name).at(arg_name));
    };
    template<>
    std::string node_args::get_arg<std::string>(const std::string & node_name,  const std::string & arg_name)const
    {
        return args_data_.at(node_name).at(arg_name); 
    };
    template<>
    bool node_args::get_arg<bool>(const std::string & node_name,  const std::string & arg_name)const
    {
        return args_data_.at(node_name).at(arg_name) == "true" ? true : false; 
    };