
#include "../../data_flow/dataflow_package.h"
#include <queue>
#include "../../data_structs/cluster.h"
#include "../../data_structs/node_args.h"
#include "../../utils.h"
#include "../../devices/current_device.h"
#include "../../benchmark/i_time_measurable.h"
#include "cluster_merger.h"
template <typename hit_type, typename clustering_node>
class parallel_clusterer : public i_data_consumer<hit_type>,
                           public i_data_producer<cluster<hit_type>>,
                           public i_time_measurable // performs the splitting work
{                                                   // and wraps the whole clustering process

    split_descriptor<hit_type> *split_descr_;
    measuring_clock *clock_;
    pipe_reader<hit_type> reader_;
    pipe_writer<cluster<hit_type>> writer_;
    cluster_merging<hit_type> *merging_node_;
    std::vector<clustering_node *> clustering_nodes_;
    std::vector<default_pipe<hit_type> *> split_pipes_;
    std::vector<pipe_writer<hit_type>> split_writers_;
    const uint32_t PIPE_ID_PREFIX = 0x100;
    std::vector<default_pipe<cluster<hit_type>> *> merging_pipes_;
    std::vector<double> first_toas_by_producers_; // used
public:
    parallel_clusterer(node_descriptor<cluster<hit_type>, hit_type> *node_descr, const node_args &args) : split_descr_(node_descr->split_descr),
                                                                                                          merging_node_(new cluster_merging<hit_type>(
                                                                                                              new node_descriptor<cluster<hit_type>, cluster<hit_type>>(node_descr->merge_descr,
                                                                                                                                                                        new trivial_split_descriptor<cluster<hit_type>>(), "merging_node_descriptor")))
    {
        for (uint32_t i = 0; i < split_descr_->pipe_count(); i++)
        {

            clustering_node *cl_node = new clustering_node(args);
            default_pipe<hit_type> *split_pipe = new default_pipe<hit_type>(name() + "_" +
                                                                            cl_node->name() + "_" + std::to_string(split_pipes_.size()));
            default_pipe<cluster<hit_type>> *merge_pipe = new default_pipe<cluster<hit_type>>(cl_node->name() + "_" +
                                                                                              merging_node_->name() + "_" + std::to_string(merging_pipes_.size()));

            cl_node->connect_input(split_pipe);
            cl_node->connect_output(merge_pipe);
            clustering_nodes_.emplace_back(cl_node);

            split_pipes_.push_back(split_pipe);
            split_writers_.emplace_back(pipe_writer<hit_type>(split_pipe));

            merging_pipes_.emplace_back(merge_pipe);
            merging_node_->connect_input(merge_pipe);
        }
    }
    virtual void connect_input(default_pipe<hit_type> *input_pipe) override
    {
        reader_ = pipe_reader<hit_type>(input_pipe);
    }
    virtual void connect_output(default_pipe<cluster<hit_type>> *output_pipe) override
    {
        writer_ = pipe_writer<cluster<hit_type>>(output_pipe);
        merging_node_->connect_output(output_pipe);
    }
    virtual std::vector<i_data_node *> internal_nodes() override
    {
        std::vector<i_data_node *> int_nodes;
        int_nodes.assign(clustering_nodes_.begin(), clustering_nodes_.end());
        int_nodes.push_back(merging_node_);
        return int_nodes;
    }
    virtual std::vector<abstract_pipe *> internal_pipes() override
    {
        std::vector<abstract_pipe *> int_pipes;
        int_pipes.assign(split_pipes_.begin(), split_pipes_.end());
        int_pipes.insert(int_pipes.end(), merging_pipes_.begin(), merging_pipes_.end());
        return int_pipes;
    }
    virtual void prepare_clock(measuring_clock *clock)
    {
        merging_node_->prepare_clock(clock);
        clock_ = clock;
    }
    std::string name() override
    {
        return "parallel_clusterer";
    }
    virtual void start() override
    {
        hit_type hit;
        reader_.read(hit);
        clock_->start();
        while (hit.is_valid())
        {
            split_writers_[split_descr_->get_pipe_index(hit)].write(std::move(hit));
            reader_.read(hit);
        }
        for (uint32_t i = 0; i < split_descr_->pipe_count(); i++)
        {
            split_writers_[i].write(hit_type::end_token());
            split_writers_[i].flush();
        }
        // std::cout << "SPLITTER ENDED ---------------------" << std::endl;
    }
    virtual ~parallel_clusterer() = default;
};
