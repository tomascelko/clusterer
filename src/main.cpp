#include "../include/data_reader.h"
#include "../include/burda_hit.h"
#include "../include/hit_sorter.h"
#include "../include/dataflow_controller.h"
#include "../include/data_printer.h"
#include "../include/clusterer.h"
#include "../include/burda_to_mm_hit_adapter.h"
#include "../include/cluster.h"
#include "../include/mm_stream.h"
int main(int argc, char** argv)
{
    const std::string filename = "/home/tomas/MFF/DT/measurement_august2022/burda_data/pions_180GeV_bias200V_60deg_alignment_test_F4-W00076_1.txt";
    data_reader<burda_hit>* burda_reader = new data_reader<burda_hit>{filename, 2 << 10};
    auto chip_size = coord(256, 256);
    burda_to_mm_hit_adapter<mm_hit>* converter = new burda_to_mm_hit_adapter<mm_hit>(chip_size, calibration("/home/tomas/MFF/DT/measurement_august2022/calib/F4-W00076/", chip_size));
    hit_sorter<mm_hit>* sorter = new hit_sorter<mm_hit>();
    
    //std::ofstream print_stream("printed_hits.txt");
    mm_write_stream print_stream("clustered_test");
    data_printer<cluster<mm_hit>, mm_write_stream>* printer = new data_printer<cluster<mm_hit>, mm_write_stream>(&print_stream);
    pixel_list_clusterer* clusterer = new pixel_list_clusterer(1024);
    dataflow_controller controller;
    controller.add_node(burda_reader);
    controller.add_node(converter);
    controller.add_node(sorter);
    controller.add_node(clusterer);
    //controller.add_node(printer);

    controller.connect_nodes(burda_reader, converter);
    controller.connect_nodes(converter, sorter);
    controller.connect_nodes(sorter, clusterer);
    //controller.connect_nodes(clusterer, printer);
    controller.start_all();
    print_stream.close();
    //TODO add virtual destructors
}