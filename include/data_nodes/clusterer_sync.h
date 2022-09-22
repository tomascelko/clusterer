
#include  "clusterer.h"
#include "../data_structs/cluster.h"
#include "../data_structs/produced_cluster.h"
class sync_pixel_list_clusterer : public pixel_list_clusterer<produced_cluster>
{
    i_sync_node* sync_node_; //has method sync_time and check_time 
    const double MAX_UNSYNC_DELAY = 20000;
    const uint32_t WAIT_DURATION = 500; //int microsec
    const uint32_t SYNC_INTERVAL = 100; //in hits
    const uint32_t MAX_QUEUE_SIZE = 100000; //in clusters inside of a merging node
    uint16_t id_;
    void check_wait(double current_toa)
    {
        sync_node_->sync_time(); //should check all other clusterers as well
        double advantage = current_toa - sync_node_->check_time();
        if(advantage > MAX_UNSYNC_DELAY)
        {
            while(sync_node_->queue_size() > MAX_QUEUE_SIZE)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(WAIT_DURATION));
            }
        }

    }
    public:
    sync_pixel_list_clusterer(i_sync_node* sync_node, uint16_t id) :
    sync_node_(sync_node),
    id_(id)
    {
        
    }
    virtual void write_old_clusters(double hit_toa = 0) override
    {
        //old clusters should be at the end of the list
        while (unfinished_clusters_count_ > 0  && (is_old(hit_toa, unfinished_clusters_.back().cl) || finished_))
        {
            unfinished_cluster<mm_hit> & current = unfinished_clusters_.back();
            auto & current_hits = current.cl.hits(); 
            for(uint32_t i = 0; i < current.pixel_iterators.size(); i++) //update iterator
            {
                auto & pixel_list_row = pixel_lists_[current_hits[i].coordinates().linearize()];
                pixel_list_row.erase(current.pixel_iterators[i]); //FIXME pixel_list_rows should be appended in other direction
            }
            current_toa_ = unfinished_clusters_.back().cl.first_toa();
            unfinished_clusters_.back().cl.set_producer_id(id_);
            writer_.write(std::move(unfinished_clusters_.back().cl));
            unfinished_clusters_.pop_back();
            --unfinished_clusters_count_;
        }
    } 
    virtual void start() override
    {
        mm_hit hit;
        reader_.read(hit);
        while(hit.is_valid())
        {
            if(processed_hit_count_ % WRITE_INTERVAL == 0) 
                write_old_clusters(hit.toa());
            if(processed_hit_count_ % SYNC_INTERVAL == 0)
                check_wait(current_toa());
            process_hit(hit);
            reader_.read(hit);
        }   
        write_remaining_clusters();
        writer_.write(produced_cluster<mm_hit>::end_token(id_)); //write empty cluster as end token
        writer_.flush();
        std::cout << "CLUSTERER ENDED ---------- " << processed_hit_count_ <<" hits processed" <<std::endl;
    }

};