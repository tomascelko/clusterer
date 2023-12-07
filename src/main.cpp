#include "../include/benchmark/benchmarker.h"
#include "../include/benchmark/i_time_measurable.h"
#include "../include/data_flow/dataflow_controller.h"
#include "../include/data_nodes/nodes_package.h"
#include "../include/data_structs/burda_hit.h"
#include "../include/data_structs/cluster.h"
#include "../include/data_structs/node_args.h"
#include "../include/execution/arg_parser.h"
#include "../include/execution/executor.h"
#include "../include/execution/model_runner.h"
#include "../include/experiments/performance/performance_test.h"
#include "../include/experiments/validation/validation.h"
#include "../include/mm_stream.h"
#include "../include/utils.h"
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
  // auto controller = udp_controller("192.168.1.153");

  // return 0;
  std::cout << "Starting the run..." << std::endl;
  std::cout << "Parsing the arguments..." << std::endl;
  std::vector<std::string> args(argv, argc + argv);
  // model_runner::print = false;

  args = {"./clusterer",
          "--debug",
          "--n_workers",
          "8",
          "--clustering_type",
          "standard",
          "--merge_type",
          "multioutput",
          "--args",
          "../../../clusterer_data/parameters/node_params.txt",
          "--calib",
          "../../../clusterer_data/calib/k07_CALIB/calib_coeffs/",
          "/home/celko/Work/repos/clusterer_data/pion/pions_180GeV_deg_0.txt"};

  argument_parser parser = argument_parser(args[0]);
  parser.try_parse_and_run(
      std::vector<std::string>(args.begin() + 1, args.end()));
  return 0;

  std::string output_folder = "../../output/";
  std::vector<std::string> data_files{
      "pion/pions_180GeV_deg_0.txt",
      "pion/pions_180GeV_deg_30.txt",
      "lead/deg0_0.txt",
      "lead/deg90_2.txt",
      "neutron/beam_T0trigger_0deg_150V_10s_0.txt",
      "neutron/neutrons_60deg_200V_F4-W00076_1.txt",
  };

  std::string binary_path = "";
  auto val_output = std::ofstream(
      binary_path +
      "../../output/benchmark_results_laptop/validation_result.txt");
  auto val_run = validation(&val_output, "", true);
  val_run.run_validation("");
}
