#include "../../comm.h"
#include "../../utils.h"
#include "data_reader.h"
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/icmp.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#pragma once

class online_data_reader : public data_reader<burda_hit, std::ifstream> {
  std::string katherine_ip_ = "192.168.1.130";
  udp_controller katherine_controller;

public:
  online_data_reader(node_descriptor<burda_hit, burda_hit> *node_descriptor,
                     const std::string &filename, const node_args &args,
                     const std::string &calib_folder = "")
      : data_reader<burda_hit, std::ifstream>(node_descriptor, filename, args),
        katherine_controller(
            katherine_ip_,
            [this](std::vector<burda_hit> &&hits) {
              this->process_packet(std::move(hits));
            },
            [this]() { this->end_acq(); }) {
    // initialize_internal_clustering(calib_folder, args);
    // init_read_data();
  }
  online_data_reader(const std::string &filename, const node_args &args,
                     const std::string &calib_folder = "")
      : data_reader<burda_hit, std::ifstream>(filename, args),
        katherine_controller(
            katherine_ip_,
            [this](std::vector<burda_hit> &&hits) {
              this->process_packet(std::move(hits));
            },
            [this]() { this->end_acq(); }) {}
  std::string name() override { return "reader"; }
  void process_packet(std::vector<burda_hit> &&hits) {
    for (auto &&hit : hits) {
      this->writer_.write(std::move(hit));
      ++this->total_hits_processed_;
    }
    this->perform_memory_check();
  }
  bool done = false;
  void end_acq() {
    std::cout << "TOTAL HITS PROCESSED: " << this->total_hits_processed_
              << std::endl;
    std::cout
        << "UDP TRANSFER SUCCESS RATE: "
        << (this->total_hits_processed_ /
            (double)this->katherine_controller.parser.total_hits_received) *
               100
        << "%" << std::endl;
    this->writer_.close();
    done = true;
  }
  virtual void start() override {
    // std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    katherine_controller.async_start_acq();
    // while (!done)
    {
      // std::cout << "Waiting" << std::endl;
      // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }
};