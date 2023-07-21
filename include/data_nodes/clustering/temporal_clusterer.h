#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_set>
#include <set>
#include <map>
#include <queue>
#include "../../data_flow/dataflow_package.h"
#include "../../utils.h"
#include "../../data_structs/mm_hit.h"
#include "../../data_structs/node_args.h"
#include "../../utils.h"
#include "../../benchmark/i_time_measurable.h"
#include "../../benchmark/measuring_clock.h"
#include <deque>
#include <list>
//a simple temporal clusterer, which only groups hits based on a temporal neighborhood
template <template <class> typename cluster>
class temporal_clusterer : public i_simple_consumer<mm_hit>,
                           public i_multi_producer<cluster<mm_hit>>
{
    double last_hit_toa_;
    double max_clustering_dt_;

public:
    std::string name()
    {
        return "clusterer";
    }
    temporal_clusterer(node_descriptor<mm_hit, cluster<mm_hit>> *node_descr, const node_args &args) : last_hit_toa_(0),
                                                                                                      max_clustering_dt_(args.get_arg<double>(name(), "max_dt")),
                                                                                                      i_multi_producer<cluster<mm_hit>>(node_descr->split_descr)
    {
    }
    temporal_clusterer(const node_args &args) : last_hit_toa_(0),
                                                max_clustering_dt_(args.get_arg<double>(name(), "max_dt")),
                                                i_multi_producer<cluster<mm_hit>>()
    {
    }
    virtual void start() override
    {
        mm_hit hit;
        cluster<mm_hit> open_cluster;
        double last_hit_toa_ = 0.;
        double epsilon = 0.0000001;
        reader_.read(hit);
        while (hit.is_valid())
        {
            //compute the time difference with previous hit
            double dt = hit.toa() - last_hit_toa_;
            //add hit to currently open cluster
            if (dt < max_clustering_dt_)
            {
                last_hit_toa_ = hit.toa();
                open_cluster.add_hit(std::move(hit));
            }
            //start a new cluster
            else
            {
                if (last_hit_toa_ > epsilon)
                    this->writer_.write(std::move(open_cluster));
                open_cluster = cluster<mm_hit>();
                last_hit_toa_ = hit.toa();
                open_cluster.add_hit(std::move(hit));
            }
            reader_.read(hit);
        }
        this->writer_.close();
    }
    virtual ~temporal_clusterer() = default;
};