#include <algorithm>
#include "../data_flow/dataflow_package.h"
#include <queue>
#include "../data_structs/burda_hit.h"

template <typename data_type>

class hit_sorter : public i_data_consumer<data_type>, public i_data_producer<data_type>
{
    struct toa_comparer
    {
        auto operator() (const data_type& left, const data_type& right) const 
        {
         if(left.toa() < right.toa())
                return false;
            if(right.toa() < left.toa())
                return true;
            return false;   
        }
    };
    //for sorting burda hits
    struct burda_toa_comparer
    {
        auto operator() (const burda_hit& left, const burda_hit& right) const 
        {
         if(left.toa() < right.toa())
                return false;
            if(right.toa() < left.toa())
                return true;
            if(left.fast_toa() < right.fast_toa())
                return false;
            if(right.fast_toa() < left.fast_toa())
                return true;
            return false;   
        }
    };
    using buffer_type = data_buffer<data_type>; 
    pipe_reader<data_type> reader_;
    pipe_writer<data_type> writer_;
    std::priority_queue<data_type, std::vector<data_type>, toa_comparer> priority_queue_;
    
    const double DEQUEUE_TIME = 500000.; // in 
    const uint32_t DEQUEUE_CHECK_INTEVAL = 512;
public:
    hit_sorter()   
    {
        toa_comparer less_comparer;
        priority_queue_ = std::priority_queue<data_type, std::vector<data_type>, toa_comparer> (less_comparer);
    }
    virtual void connect_input(pipe<data_type>* input_pipe) override
    {
        reader_ = pipe_reader<data_type>(input_pipe);
    }
    virtual void connect_output(pipe<data_type>* output_pipe) override
    {
        writer_ = pipe_writer<data_type>(output_pipe);
    }
    virtual void start() override
    {
        data_type hit;
        int processed = 0;
        while(!reader_.read(hit));
        while(hit.is_valid())
        { //data not end
            processed ++;
            priority_queue_.push(hit);
            
            if(processed % DEQUEUE_CHECK_INTEVAL == 0)
                while (priority_queue_.top().toa() < hit.toa() - DEQUEUE_TIME)
                {
                    data_type old_hit = priority_queue_.top();
                    writer_.write(std::move(old_hit));
                    priority_queue_.pop();
                }
            reader_.read(hit);

        
        }
        while (!priority_queue_.empty())
            {
                data_type old_hit = priority_queue_.top();
                priority_queue_.pop();
                writer_.write(std::move(old_hit));
            }
        //write last (invalid) hit
        writer_.write(std::move(hit));
        writer_.flush();
        std::cout << "HIT SORTER ENDED ---------------------" << std::endl;
        


    }
};