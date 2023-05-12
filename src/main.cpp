#include "../include/data_structs/burda_hit.h"
#include "../include/data_flow/dataflow_controller.h"
#include "../include/data_nodes/nodes_package.h"
#include "../include/data_structs/cluster.h"
#include "../include/data_structs/node_args.h"
#include "../include/mm_stream.h"
#include "../include/utils.h"
#include "../include/benchmark/i_time_measurable.h"
#include "../include/benchmark/benchmarker.h"
#include "../include/execution/executor.h"
#include "../include/execution/model_selector.h"

#include <iostream>
#include <fstream>
#include "onnxruntime_cxx_api.h"
// #include <cppflow/cppflow.h>
using standard_clustering_type = pixel_list_clusterer<cluster>;
using parallel_clusterer_type = parallel_clusterer<mm_hit, pixel_list_clusterer<cluster>>;
// pretty prints a shape dimension vector
std::string print_shape(const std::vector<std::int64_t> &v)
{
    std::stringstream ss("");
    for (std::size_t i = 0; i < v.size() - 1; i++)
        ss << v[i] << "x";
    ss << v[v.size() - 1];
    return ss.str();
}

int calculate_product(const std::vector<std::int64_t> &v)
{
    int total = 1;
    for (auto &i : v)
        total *= i;
    return total;
}

template <typename T>
Ort::Value vec_to_tensor(std::vector<T> &data, const std::vector<std::int64_t> &shape)
{
    Ort::MemoryInfo mem_info =
        Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
    auto tensor = Ort::Value::CreateTensor<T>(mem_info, data.data(), data.size(), shape.data(), shape.size());
    return tensor;
}

void testOnnx()
{
    std::string model_name = "/home/tomas/MFF/DT/window_trigger_creator/output/abcd.nnt";
    std::basic_string<ORTCHAR_T> model_file = model_name;

    // onnxruntime setup
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "example-model-explorer");
    Ort::SessionOptions session_options;
    Ort::Session session = Ort::Session(env, model_file.c_str(), session_options);

    // print name/shape of inputs
    Ort::AllocatorWithDefaultOptions allocator;
    std::vector<std::string> input_names;
    std::vector<std::int64_t> input_shapes;
    std::cout << "Input Node Name/Shape (" << input_names.size() << "):" << std::endl;
    for (std::size_t i = 0; i < session.GetInputCount(); i++)
    {
        input_names.emplace_back(session.GetInputNameAllocated(i, allocator).get());
        input_shapes = session.GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
        std::cout << "\t" << input_names.at(i) << " : " << print_shape(input_shapes) << std::endl;
    }
    // some models might have negative shape values to indicate dynamic shape, e.g., for variable batch size.
    for (auto &s : input_shapes)
    {
        if (s < 0)
        {
            s = 1;
        }
    }

    // print name/shape of outputs
    std::vector<std::string> output_names;
    std::cout << "Output Node Name/Shape (" << output_names.size() << "):" << std::endl;
    for (std::size_t i = 0; i < session.GetOutputCount(); i++)
    {
        output_names.emplace_back(session.GetOutputNameAllocated(i, allocator).get());
        auto output_shapes = session.GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
        std::cout << "\t" << output_names.at(i) << " : " << print_shape(output_shapes) << std::endl;
    }

    // Assume model has 1 input node and 1 output node.
    assert(input_names.size() == 1 && output_names.size() == 1);

    // Create a single Ort tensor of random numbers
    auto input_shape = input_shapes;
    auto total_number_elements = calculate_product(input_shape);

    // generate random numbers in the range [0, 255]
    std::vector<double> input_tensor_values(total_number_elements);
    std::generate(input_tensor_values.begin(), input_tensor_values.end(), [&]
                  { return rand() % 255; });
    std::vector<Ort::Value> input_tensors;
    input_tensors.emplace_back(vec_to_tensor<double>(input_tensor_values, input_shape));

    // double-check the dimensions of the input tensor
    assert(input_tensors[0].IsTensor() && input_tensors[0].GetTensorTypeAndShapeInfo().GetShape() == input_shape);
    std::cout << "\ninput_tensor shape: " << print_shape(input_tensors[0].GetTensorTypeAndShapeInfo().GetShape()) << std::endl;

    // pass data through model
    std::vector<const char *> input_names_char(input_names.size(), nullptr);
    std::transform(std::begin(input_names), std::end(input_names), std::begin(input_names_char),
                   [&](const std::string &str)
                   { return str.c_str(); });

    std::vector<const char *> output_names_char(output_names.size(), nullptr);
    std::transform(std::begin(output_names), std::end(output_names), std::begin(output_names_char),
                   [&](const std::string &str)
                   { return str.c_str(); });

    std::cout << "Running model..." << std::endl;
    try
    {
        auto output_tensors = session.Run(Ort::RunOptions{nullptr}, input_names_char.data(), input_tensors.data(),
                                          input_names_char.size(), output_names_char.data(), output_names_char.size());
        std::cout << "Done!" << std::endl;

        // double-check the dimensions of the output tensors
        // NOTE: the number of output tensors is equal to the number of output nodes specifed in the Run() call
        assert(output_tensors.size() == output_names.size() && output_tensors[0].IsTensor());
        double trigger_prob = output_tensors[0].GetTensorMutableData<double>()[0];
        std::cout << trigger_prob << std::endl;
    }
    catch (const Ort::Exception &exception)
    {
        std::cout << "ERROR running model inference: " << exception.what() << std::endl;
        exit(-1);
    }
}

int main(int argc, char **argv)
{
    //testOnnx();
    const uint32_t TILE_SIZE = 2;
    const uint32_t TIME_WINDOW_SIZE = 50000000; // 50ms
    std::vector<std::string> args(argv + 1, argc + argv);
    uint32_t core_count = args.size() > 0 ? std::stoi(args[0]) : 4;
    std::string freq_multiplier = args.size() > 1 ? args[1] : "1";
    std::cout << "created" << std::endl;
    args = {"../../test_data/small/", "../../test_data/calib/", "pmm", "4"};
    std::string data_folder = args[0];
    std::vector<std::string> cluster_comparison_datasets = {
        "/home/tomas/MFF/DT/clusterer/output/clustered_0_06-05-2023_19_44_5706-05-2023-19-44-57.ini",
        "/home/tomas/MFF/DT/clusterer/test_data/validation/small/\
Clustered - pions_180GeV_bias200V_60deg_alignment_test_F4-W00076_1.txt - 19-47 28-12-2022.ini"};
    std::string calib_folder = args[1];
    std::string arch_type = args[2];
    //uint16_t clustering_cores = std::stoi(args[3]);

    std::string output_folder = "../../output/";
    std::string data_file = "../../test_data/small/pions_180GeV_bias200V_60deg_alignment_test_F4-W00076_1.txt";
    node_args n_args;
    //n_args["trigger"]["trigger_file"] = "../../test_data/trigger/interval/test.ift";
    n_args["trigger"]["trigger_file"] =  "../../test_data/trigger/nn/test3.nnt";
    n_args["trigger"]["data_file"] = "../../test_data/trigger/nn/window_features_test.wf";
    
    n_args["reader"]["freq_multiplier"] = freq_multiplier;

    benchmarker *bench = new benchmarker(data_folder, calib_folder, "../../output/");
      //model_executor * comp_executor = new model_executor(cluster_comparison_datasets,
      //std::vector<std::string>(), output_folder);
    model_executor *executor = new model_executor(std::vector<std::string>{data_file},
                                                  calib_folder, output_folder);


    model_selector::print = false;
    model_selector::recurring = true;
    std::cout << "selecting model" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    model_selector::select_model(model_selector::model_name::TRIGGER_SIMPLE_CLUSTERER, executor, core_count, "", n_args);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << "Runtime: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms" << std::endl;
    std::cout << "ALL NODES ENDED" << std::endl;
    // delete bench;
    delete executor;
    //TODO add percentage of filtered hits by trigger
}
