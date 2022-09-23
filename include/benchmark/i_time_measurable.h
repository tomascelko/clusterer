
#include "utils.h"
#include <string>
#include <map>
#include <vector>
#include "../dataflow_package.h"
class exec_time_result
{
    private:
    std::string node_name_;
    double exec_time_;
    public:
    double exec_time()

};
class benchmark_config
{
    private:
    uint32_t run_id_;
    std::string 
};
class benchmarker
{
    using node_id_type = std::string;
    using dataset_name_type = std::string;
    using measured_runs_type = std::vector<double>;
    using measured_dataset_matrix_type = std::map<dataset_name_type, measured_runs_type>;
    using full_benchmark_type = std::map<node_id_type, measured_dataset_type>;
    dataflow_controller * controller_;
    std::vector<file_path> data_files_;

    std::map<node_id_type, measured_runs_type>
    public:
    node_benchmarker(dataflow_controller* controller, path folder) :
    {
        folder.get_all_datasets();
    }
    void register_result(exec_time_result result)
    {
        //TODO WHEN MEASURING, SKIP THE READ  OPERATION
        //IDEALLY SKIP WRITE AS WELL

    }

}
class i_time_measurable
{
    public:
    bool is_done() = 0;
    prepare_clock(std::function<void, exec_time_result> callback) = 0; //recommended to be called before start method of a node
    


};