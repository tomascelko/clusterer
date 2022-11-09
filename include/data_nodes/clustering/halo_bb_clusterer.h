
#include "clusterer.h"
#include "../../data_structs/cluster.h"
template<typename hit_type>
class halo_buffer_clusterer : public i_simple_consumer<hit_type>, public i_simple_producer<cluster<hit_type>>, public i_time_measurable
{ 
    pixel_list_clusterer<cluster> clustering_node_;
    measuring_clock * clock_;
    virtual void start() override
    {
        hit_type hit;
        reader_.read(hit);
        clock_->start();
        while(hit.is_valid())
        {
            if(processed_hit_count_ % WRITE_INTERVAL == 0) 
                write_old_clusters(hit.toa());
            process_hit(hit);
            reader_.read(hit);
        }   
        write_remaining_clusters();
        clock_->stop_and_report("clusterer");
        this->writer_.write(cluster<mm_hit>::end_token()); //write empty cluster as end token
        this->writer_.flush();

        std::cout << "CLUSTERER ENDED ---------- " << processed_hit_count_ <<" hits processed" <<std::endl;
    }
    void process_buffer(std::vector<hit_type> & buffer)
    {
        //TODO API for trigger clusterer to call 
    }
    virtual ~halo_buffer_clusterer() = default;
    virtual void prepare_clock(measuring_clock* clock) override
    {
        clock_ = clock;
    }
};