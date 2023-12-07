#include "../../data_flow/dataflow_package.h"
#include "../../data_structs/mm_hit.h"
#include "../../data_structs/node_args.h"
#include "../../devices/current_device.h"
#include "../../utils.h"
#include "cluster_merger.h"
#include <cstdint>
#include <stack>
#include <unistd.h>
class cluster_splitter : public i_simple_consumer<mm_hit>,
                         public i_multi_producer<cluster<mm_hit>> {
  using cluster_it = std::vector<cluster<mm_hit>>::iterator;

  struct partitioned_hit {
    double toa;
    coord coordinates;
    uint32_t partition_index;

    ushort x() { return coordinates.x(); }

    ushort y() { return coordinates.y(); }

    partitioned_hit(double toa, const coord &coordinates)
        : toa(toa), partition_index(0), coordinates(coordinates) {}
  };

  struct partition_time_pair {
    uint32_t partition_index;
    double toa;

    partition_time_pair(double toa, uint32_t partition_index)
        : toa(toa), partition_index(partition_index) {}
  };

  class pixel_matrix

  {
    std::array<std::array<std::vector<partitioned_hit>,
                          current_chip::chip_type::size_x() + 2>,
               current_chip::chip_type::size_y() + 2>
        pixel_matrix_;

  public:
    std::vector<partitioned_hit> &at(uint32_t i, uint32_t j) {
      return pixel_matrix_[i + 1][j + 1];
    }
    const std::vector<partitioned_hit> &at(uint32_t i, uint32_t j) const {
      return pixel_matrix_[i + 1][j + 1];
    }
  };

  using timestamp_it = std::vector<partitioned_hit>::iterator;

  const std::vector<coord> EIGHT_NEIGHBORS = {{-1, -1}, {-1, 0}, {-1, 1},
                                              {0, -1},  {0, 0},  {0, 1},
                                              {1, -1},  {1, 0},  {1, 1}};
  pixel_matrix pixel_matrix_;
  std::vector<timestamp_it> timestamp_references_;
  std::vector<cluster<mm_hit>> result_clusters_;
  u_int64_t clusters_procesed_ = 0;
  const uint64_t QUEUE_CHECK_INTERVAL = 16;
  const double MAX_JOIN_TIME = 200.;
  // std::vector<partition_time_pair> partition_time_pairs_;
  std::vector<cluster<mm_hit>> temp_clusters_;

  void store_to_matrix(const cluster<mm_hit> &cluster) {
    const uint64_t MAX_SAME_PIXEL_COUNT = 10;
    for (const auto &hit : cluster.hits()) {
      pixel_matrix_.at(hit.x(), hit.y())
          .reserve(std::min(cluster.hits().size(), MAX_SAME_PIXEL_COUNT));
      pixel_matrix_.at(hit.x(), hit.y())
          .emplace_back(hit.toa(), hit.coordinates());
      timestamp_references_.push_back(pixel_matrix_.at(hit.x(), hit.y()).end() -
                                      1);
    }
  }

  void remove_from_matrix() {
    for (const auto &cluster : temp_clusters_)
      for (const auto &hit : cluster.hits()) {
        pixel_matrix_.at(hit.x(), hit.y()).clear();
      }
    timestamp_references_.clear();
    write_buffered();
    // temp_clusters_.clear();
  }
  std::stack<timestamp_it> open_nodes;
  void dfs_tag(timestamp_it node, uint32_t current_partition_index) {
    // std::stack<timestamp_it> open_nodes;
    open_nodes.push(node);

    node->partition_index = current_partition_index;
    while (!open_nodes.empty()) {
      auto current_node = open_nodes.top();
      open_nodes.pop();

      for (const auto &neighbor_offset : EIGHT_NEIGHBORS) {
        auto &neighbor_data_vector = pixel_matrix_.at(
            current_node->coordinates.x() + neighbor_offset.x(),
            current_node->coordinates.y() + neighbor_offset.y());
        if (neighbor_data_vector.size() > 0) {
          for (timestamp_it neighbor_it = neighbor_data_vector.begin();
               neighbor_it != neighbor_data_vector.end(); ++neighbor_it) {
            if (neighbor_it->partition_index == 0 &&
                std::abs(neighbor_it->toa - current_node->toa) <
                    MAX_JOIN_TIME) {
              // timestamp_it it_copy;
              // it_copy = neighbor_it;
              open_nodes.push(neighbor_it);
              neighbor_it->partition_index = current_partition_index;
            }
          }
        }
      }

      // TODO PAD matrix by 1 columns around to never be outside of array
    }
  }

  void label_components(const cluster<mm_hit> &cluster) {
    uint32_t current_partition_index = 1;
    for (uint32_t i = 0; i < timestamp_references_.size(); ++i) {
      if (timestamp_references_[i]->partition_index == 0) {
        dfs_tag(timestamp_references_[i], current_partition_index);
        ++current_partition_index;
      }
    }
  }

  void split_cluster(cluster<mm_hit> &&cluster) {
    auto buffered_cluster_count = result_clusters_.size();
    /*for (uint32_t i = 0; i < timestamp_references_.size(); ++i)
    {
      while (partition_time_pairs_.size() <=
             timestamp_references_[i]->partition_index)
        partition_time_pairs_.emplace_back(partition_time_pairs_.size() + 1,
                                           std::numeric_limits<double>::max());
      if (partition_time_pairs_[timestamp_references_[i]->partition_index].toa >
          timestamp_references_[i]->toa)
        partition_time_pairs_[timestamp_references_[i]->partition_index].toa =
            timestamp_references_[i]->toa;
    }*/
    for (uint32_t i = 0; i < timestamp_references_.size(); ++i) {
      auto current_cluster_index =
          timestamp_references_[i]->partition_index - 1;
      while (current_cluster_index >= temp_clusters_.size())
        temp_clusters_.emplace_back();
      temp_clusters_[current_cluster_index].add_hit(
          std::move(cluster.hits()[i]));
    }
    if (temp_clusters_.size() > 1)
      std::sort(temp_clusters_.begin(), temp_clusters_.end(),
                [](const auto &left, const auto &right) {
                  return left.first_toa() < right.first_toa();
                });
    clusters_procesed_ += temp_clusters_.size();

    // result_clusters_.insert(result_clusters_.end(), temp_clusters_.begin(),
    //                         temp_clusters_.end());
  }

public:
  void process_cluster(cluster<mm_hit> &&cluster) {

    if (cluster.size() > 1) {
      bbox bb{cluster};
      if (bb.area() > 25 || bb.area() / cluster.size() > 5) {
        store_to_matrix(cluster);
        label_components(cluster);
        split_cluster(std::move(cluster));
        remove_from_matrix();
      } else {
        writer_.write(std::move(cluster));
      }
    } else {
      writer_.write(std::move(cluster));
    }
  }

  std::string name() override { return "two_step_clusterer"; }
  cluster_splitter(node_descriptor<mm_hit, cluster<mm_hit>> *node_descr)
      : result_clusters_(),
        i_multi_producer<cluster<mm_hit>>(node_descr->split_descr){};

  std::vector<cluster<mm_hit>> process_remaining() {
    std::cout << "Processed clusters: " << clusters_procesed_ << std::endl;
    return result_clusters_;
  }
  void write_buffered() {
    for (uint64_t i = 0; i < temp_clusters_.size(); ++i) {
      writer_.write(std::move(temp_clusters_[i]));
    }
    temp_clusters_.clear();
  }
  cluster<mm_hit> open_cluster;
  double last_hit_toa_ = 0;
  bool should_create_new_cluster(const mm_hit &hit) {
    double dt = hit.toa() - last_hit_toa_;
    // add hit to currently open cluster
    if (dt < MAX_JOIN_TIME && last_hit_toa_ > 0.001) {
      last_hit_toa_ = hit.toa();
      return false;
    }
    last_hit_toa_ = hit.toa();
    return true;
  }

  void start() {
    mm_hit hit;
    reader_.read(hit);
    while (hit.is_valid()) {
      if (should_create_new_cluster(hit)) {
        process_cluster(std::move(open_cluster));
        open_cluster = cluster<mm_hit>();
        ++clusters_procesed_;
      }
      open_cluster.add_hit(std::move(hit));
      reader_.read(hit);
    }
    // write_buffered();
    writer_.close();
    std::cout << "Produced " << clusters_procesed_ << " clusters" << std::endl;
  }

  std::vector<cluster<mm_hit>> &result_clusters() { return result_clusters_; }
};