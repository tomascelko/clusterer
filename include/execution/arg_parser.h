#include <exception>
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include "../data_structs/node_args.h"
#include "../benchmark/benchmarker.h"
#include "../execution/executor.h"
#include "../execution/model_runner.h"
#include "../experiments/performance/performance_test.h"
#include "../utils.h"
//class which handles parsing of command line arguments
class argument_parser
{
    //cmd line options with modes
    const std::string HELP_OPTION_STR = "--help";
    const std::string NODE_ARGS_OPTION_STR = "--args";
    const std::string CALIB_OPTION_STR = "--calib";
    const std::string MODE_OPTION_STR = "--mode";
    const std::string OUTPUT_OPTION_STR = "--output";
    const std::string DEBUG_OPTION_STR = "--debug";
    const std::string CORE_COUNT_OPTION_STR = "--n_workers";
    const std::string CLUSTERING_TYPE_OPTION_STR = "--clustering_type";
    const std::string MERGE_TYPE_OPTION_STR = "--merge_type";
    const std::string MODE_CLUSTERING = "clustering";
    const std::string MODE_WINDOW_FEATURES = "window_features";
    const std::string MODE_BENCHMARK = "benchmark";
    const std::string MODE_COMPARISON = "compare";
    bool debug_ = false;
    std::string path_to_binary_;

private:
    //parse the node_args file
    node_args load_node_args(const std::string &node_arg_filename)
    {
        node_args args;
        const char property_mark = '-';
        const char key_value_separator = ':';
        std::ifstream node_args_stream(node_arg_filename);
        if (!node_args_stream.is_open())
        {
            throw std::invalid_argument("File: '" + node_arg_filename + "' cannot be read");
        }
        std::string line;
        std::string current_node = "";
        while (std::getline(node_args_stream, line))
        {
            auto stripped_line = io_utils::strip(line);
            //skip empty lines
            if (stripped_line == "")
                continue;
            //the line contains node property
            if (stripped_line[0] == property_mark)
            {
                if (current_node == "")
                    throw std::invalid_argument("Invalid format of the node args file");
                auto key_value_pair = stripped_line.substr(1);
                auto delim_pos = stripped_line.find(key_value_separator);
                auto key = io_utils::strip(key_value_pair.substr(0, delim_pos - 1));
                auto value = io_utils::strip(key_value_pair.substr(delim_pos));
                args[current_node][key] = value;
            }
            //the line contains name of the node
            else
            {
                current_node = stripped_line;
            }
        }
        return args;
    }
    void display_help() const
    {
        std::cout << "Usage: <clusterer_binary> [options] <input_file1> [<input_file2>]" << std::endl;
        std::cout << "The options are: " << std::endl;
        std::cout << "--mode <one of 'clustering', 'window_features', 'compare', 'benchmark'>" << std::endl;
        std::cout << "  ->defaults to 'clustering'" << std::endl;
        std::cout << "  ->mandatory arguments if 'clustering' or 'window_features': {--calib}" << std::endl;
        std::cout << "--help" << std::endl;
        std::cout << "--args <path_to_node_argument_file>" << std::endl;
        std::cout << "--calib <path_to_calibration_file>" << std::endl;
        std::cout << "--output <path_to_output_folder>" << std::endl;
        std::cout << "  ->defaults to <binary_location>/../../output" << std::endl;
        std::cout << "--debug" << std::endl;
        std::cout << "--n_workers <positive integer>" << std::endl;
        std::cout << "  ->defaults to 4" << std::endl;
        std::cout << "--clustering_type <one of 'standard', 'temporal', 'tiled', 'halo_bb'>" << std::endl;
        std::cout << "  ->defaults to 'standard" << std::endl;
        std::cout << "--merge_type <one of 'none', 'simple', 'single_layer', 'tree' or multioutput>" << std::endl;
        std::cout << "  ->defaults to 'single_layer'" << std::endl;
    }
    //retrieves command line argument by option(key)
    std::string parse_by_key(const std::string &key, const std::vector<std::string> args) const
    {
        bool found_key = false;
        std::vector<std::string>::const_iterator key_it;
        for (auto it = args.begin(); it != args.end(); ++it)
        {
            if (key == *it)
            {
                found_key = true;
                key_it = it;
                break;
            }
        }
        if (found_key)
        {
            if (key_it == args.end())
                throw std::invalid_argument("The value for required argument '" + key + "'was not found");
            return *(++key_it);
        }
        else
        {
            throw std::invalid_argument("The value for required argument '" + key + "'was not found");
        }
    }
    //verify that file exists
    void check_existence(const std::string &file) const
    {
        if (!std::filesystem::exists(file))
        {
            throw std::invalid_argument("The file '" + file + "' does not exist");
        }
    }
    //parse output folder from cmd line
    std::string get_output_folder(const std::vector<std::string> &args) const
    {

        std::string output_folder = file_path(path_to_binary_).parent() + "/../../output/";
        if (std::find(args.begin(), args.end(), OUTPUT_OPTION_STR) != args.end())
        {
            std::string output_folder = parse_by_key(OUTPUT_OPTION_STR, args);
            std::replace(output_folder.begin(), output_folder.end(), '\\', '/');
            check_existence(output_folder);
            if (output_folder.back() != '/')
                output_folder += '/';
        }
        return output_folder;
    }
    //parse core count from cmd line
    uint32_t get_core_count(const std::vector<std::string> &args) const
    {
        uint32_t core_count = 4;
        if (std::find(args.begin(), args.end(), CORE_COUNT_OPTION_STR) != args.end())
        {
            std::string core_count_str = "";
            try
            {
                core_count_str = parse_by_key(CORE_COUNT_OPTION_STR, args);
                int core_count = std::stoi(core_count_str);
                if (core_count < 1)
                    throw std::invalid_argument("");
                return core_count;
            }
            catch (std::invalid_argument &ex)
            {
                ex = std::invalid_argument("Could not parse '" + core_count_str + "' for argument core count");
                throw ex;
            }
        }
        return core_count;
    }
    //parse clustering type (the type of clustering node)
    model_runner::clustering_type get_clustering_type(const std::vector<std::string> &args) const
    {
        if (std::find(args.begin(), args.end(), CLUSTERING_TYPE_OPTION_STR) == args.end())
            return model_runner::clustering_type::STANDARD;
        auto clustering_type = parse_by_key(CLUSTERING_TYPE_OPTION_STR, args);
        if (clustering_type == "standard")
        {
            return model_runner::clustering_type::STANDARD;
        }
        else if (clustering_type == "temporal")
        {
            return model_runner::clustering_type::TEMPORAL;
        }
        else if (clustering_type == "halo_bb")
        {
            return model_runner::clustering_type::HALO_BB;
        }
        else if (clustering_type == "tiled")
        {
            return model_runner::clustering_type::TILED;
        }
        else
        {
            throw std::invalid_argument("Given clustering type '" + clustering_type + "' was not found");
        }
    }
    //parse merging architecture
    model_runner::model_name get_model_arch(const std::vector<std::string> &args) const
    {
        if (std::find(args.begin(), args.end(), MERGE_TYPE_OPTION_STR) == args.end())
            return model_runner::model_name::PAR_LINEAR_MERGER;
        auto merge_type = parse_by_key(MERGE_TYPE_OPTION_STR, args);
        if (merge_type == "none")
        {
            return model_runner::model_name::SIMPLE_CLUSTERER;
        }
        else if (merge_type == "simple")
        {
            return model_runner::model_name::PAR_SIMPLE_MERGER;
        }
        else if (merge_type == "single_layer")
        {
            return model_runner::model_name::PAR_LINEAR_MERGER;
        }
        else if (merge_type == "tree")
        {
            return model_runner::model_name::PAR_TREE_MERGER;
        }
        else if (merge_type == "multioutput")
        {
            return model_runner::model_name::PAR_MULTIFILE_CLUSTERER;
        }
        else
        {
            throw std::invalid_argument("Given model type '" + merge_type + "' was not found");
        }
    }
    //parse all cmd line arguments
    void parse(const std::vector<std::string> &args)
    {
        if (args.size() == 0)
            throw std::invalid_argument("No command line arguments passed");
        if (std::find(args.begin(), args.end(), HELP_OPTION_STR) != args.end())
        {
            display_help();
            return;
        }
        if (std::find(args.begin(), args.end(), DEBUG_OPTION_STR) != args.end())
        {
            debug_ = true;
        }
        node_args n_args;
        if (std::find(args.begin(), args.end(), NODE_ARGS_OPTION_STR) != args.end())
        {
            auto node_args_file = parse_by_key(NODE_ARGS_OPTION_STR, args);
            check_existence(node_args_file);
            n_args = load_node_args(node_args_file);
        }
        std::string mode = "clustering";
        if (std::find(args.begin(), args.end(), MODE_OPTION_STR) != args.end())
            mode = parse_by_key(MODE_OPTION_STR, args);

        auto output_folder = get_output_folder(args);
        auto core_count = get_core_count(args);
        if (mode == MODE_COMPARISON)
        {
            if (args.size() < 2)
            {
                throw std::invalid_argument("Insufficient argument count for this option");
            }
            std::string input_file1 = args.back();
            std::string input_file2 = *(args.end() - 2);
            check_existence(input_file1);
            check_existence(input_file2);

            std::vector<std::string> output_files;
            model_executor executor = model_executor(std::vector<std::string>{input_file1, input_file2},
                                                     std::vector<std::string>{}, output_folder);
            auto output_from_run = model_runner::run_model(model_runner::model_name::VALIDATION, &executor, core_count, n_args, debug_);
            // run comparison
        }
        else if (mode == MODE_WINDOW_FEATURES)
        {
            auto calib_file = parse_by_key(CALIB_OPTION_STR, args);
            check_existence(calib_file);
            std::string input_file = args.back();
            check_existence(input_file);
            model_executor executor = model_executor(std::vector<std::string>{input_file},
                                                     std::vector<std::string>{calib_file}, output_folder);
            auto output_from_run = model_runner::run_model(model_runner::model_name::WINDOW_COMPUTER,
                                                           &executor, core_count, n_args, debug_);
        }
        else if (mode == MODE_BENCHMARK)
        {
            std::string data_folder = args.back();
            check_existence(data_folder);

            auto output_file = output_folder + "benchmark_result.txt";
            auto perf_output = std::ofstream(output_file);
            auto perf_test = performance_test(&perf_output, debug_);
            perf_test.run_all_benchmarks(path_to_binary_, data_folder, core_count);
        }
        else
        {
            auto calib_file = parse_by_key(CALIB_OPTION_STR, args);
            check_existence(calib_file);
            std::string input_file = args.back();
            check_existence(input_file);
            model_executor executor = model_executor(std::vector<std::string>{input_file},
                                                     std::vector<std::string>{calib_file}, output_folder);
            auto output_from_run = model_runner::run_model(get_model_arch(args),
                                                           &executor, core_count, n_args, debug_, get_clustering_type(args));
        }
    }

public:
    argument_parser(const std::string &path_to_binary) : path_to_binary_(path_to_binary) {}
    //wrapper around the parse method
    void try_parse_and_run(const std::vector<std::string> &args)
    {
        try
        {
            parse(args);
        }
        catch (const std::invalid_argument &ex)
        {
            std::cout << "Error occured: " << ex.what() << std::endl;
            std::cout << "Use option --help to show command line options" << std::endl;
        }
    }
};