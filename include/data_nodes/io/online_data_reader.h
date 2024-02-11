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

template <typename descriptor_type = temporal_hit_split_descriptor<burda_hit>>
class online_data_reader
    : public data_reader<burda_hit, std::ifstream, descriptor_type> {

  std::string katherine_ip_ = "192.168.1.130";
  udp_controller katherine_controller;

public:
  online_data_reader(descriptor_type *node_descriptor,
                     const std::string &filename, const node_args &args,
                     const std::string &calib_folder = "")
      : katherine_controller(
            katherine_ip_,
            [this](std::vector<burda_hit> &&hits) {
              this->process_packet(std::move(hits));
            },
            [this]() { this->end_acq(); }),
        data_reader<burda_hit, std::ifstream, descriptor_type>(node_descriptor,
                                                               filename, args) {
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
      this->last_processed_timestamp_ = hit.time();
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