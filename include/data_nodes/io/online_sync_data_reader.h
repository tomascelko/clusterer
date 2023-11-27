#include "data_reader.h"
#include "../../utils.h"
#include "../../comm.h"
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/asio/ip/icmp.hpp>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>
#pragma once
//reader capable of offsetting hits and modifying frequency of data stream
using namespace std::chrono_literals;

class online_sync_data_reader : public data_reader<burda_hit, std::ifstream>
{
    std::string katherine_ip_ = "192.168.1.130";
    udp_controller katherine_controller;
    
    //runs the clustering on hits, 
    //in order to find first hit in cluster and do the frequency scaling
    //while preserving cluster timespan
    
    
public:
    online_sync_data_reader(node_descriptor<burda_hit, burda_hit> *node_descriptor,
        const std::string &filename, const node_args &args, const std::string &calib_folder = "") 
            : data_reader<burda_hit, std::ifstream>(node_descriptor, filename, args),
              katherine_controller(katherine_ip_, [this](){this->end_acq();})                                                                                                                                                                                                                   
    {
        //initialize_internal_clustering(calib_folder, args);
        //init_read_data();
    }
    online_sync_data_reader(const std::string &filename, const node_args &args, const std::string &calib_folder = "") :
     data_reader<burda_hit, std::ifstream>(filename, args),
     katherine_controller(katherine_ip_, [this](){this->end_acq();}) 
    {}
    std::string name() override
    {
        return "reader";
    }
    void process_packet(std::vector<burda_hit> && hits)
    {
        for(auto && hit : hits)
        {
            this->writer_.write(std::move(hit));
            ++this->total_hits_processed_;
            this->perform_memory_check();
        }
    }
    bool done = false;
    void end_acq()
    {
        done = true;
    }
    virtual void start() override
    {      
        std::this_thread::sleep_for(std::chrono::milliseconds(2000)); 
        katherine_controller.sync_start_acq();
        while (!done)
        {
            process_packet(katherine_controller.sync_read_data());
        }
        this->writer_.close();
        std::cout << "TOTAL HITS PROCESSED: " << this->total_hits_processed_ << std::endl;
        std::cout << "UDP TRANSFER SUCCESS RATE: " << (this->total_hits_processed_ / 
            (double) this->katherine_controller.parser.total_hits_received) * 100 << "%" << std::endl;
    }
};