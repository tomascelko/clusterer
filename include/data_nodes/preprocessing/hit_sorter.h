#include <algorithm>
#include "../../data_flow/dataflow_package.h"
#include <queue>
#include "../../data_structs/burda_hit.h"
//uses heap sorting (priority queue) to guarantee temporal orderedness of hits
template <typename data_type>
class hit_sorter : public i_simple_consumer<data_type>,
                   public i_multi_producer<data_type>
{
    struct toa_comparer
    {
        auto operator()(const data_type &left, const data_type &right) const
        {
            if (left.toa() < right.toa())
                return false;
            if (right.toa() < left.toa())
                return true;
            return false;
        }
    };
    // for sorting burda hits temporally
    struct burda_toa_comparer
    {
        auto operator()(const burda_hit &left, const burda_hit &right) const
        {
            if (left.toa() < right.toa())
                return false;
            if (right.toa() < left.toa())
                return true;
            if (left.fast_toa() < right.fast_toa())
                return false;
            if (right.fast_toa() < left.fast_toa())
                return true;
            return false;
        }
    };
    using buffer_type = data_buffer<data_type>;
    std::priority_queue<data_type, std::vector<data_type>, toa_comparer> priority_queue_;
    //maximal unorderedness of the datastream
    const double DEQUEUE_TIME = 500000.; // in
    //check for outputting the hits every DEQUEUE_CHECK_INTERVAL hits
    const uint32_t DEQUEUE_CHECK_INTEVAL = 128;
    using split_descriptor_type = split_descriptor<data_type>;

public:
    hit_sorter()
    {
        toa_comparer less_comparer;
        priority_queue_ = std::priority_queue<data_type, std::vector<data_type>, toa_comparer>(less_comparer);
    }
    std::string name() override
    {
        return "hit_sorter";
    }
    hit_sorter(node_descriptor<data_type, data_type> *node_descriptor) : i_multi_producer<data_type>(node_descriptor->split_descr)
    {
        // assert((std::is_same<data_type, mm_hit_tot>::value));
    }
    virtual void start() override
    {
        data_type hit;
        int processed = 0;
        this->reader_.read(hit);
        int x = 0;
        while (hit.is_valid())
        { // data not end
            processed++;
            priority_queue_.push(hit);

            if (processed % DEQUEUE_CHECK_INTEVAL == 0)
                while (priority_queue_.top().toa() < hit.toa() - DEQUEUE_TIME)
                {
                    data_type old_hit = priority_queue_.top();
                    this->writer_.write(std::move(old_hit));
                    priority_queue_.pop();
                }

            this->reader_.read(hit);
        }
        //remove the remaining hits at the end of datastream
        while (!priority_queue_.empty())
        {
            data_type old_hit = priority_queue_.top();
            priority_queue_.pop();
            this->writer_.write(std::move(old_hit));
        }
        // write last (invalid) hit token and cose the stream
        this->writer_.close();
        
        
    }

    virtual ~hit_sorter() = default;
};