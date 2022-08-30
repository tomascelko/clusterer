
#include "abstract_pipe.h"
#include "readerwriterqueue.h"
#include "concurrentqueue.h"
#pragma once
template <typename data_type>
class pipe : public abstract_pipe
{
    //todo use simple one producer one consumer queue and create multi pipe : public pipe with this multi concurrent queue
private:
    moodycamel::ReaderWriterQueue<data_type> queue_;
public: 
    static constexpr uint32_t MAX_Q_LEN = 2 << 18;
    pipe() :
    queue_(MAX_Q_LEN){}
    uint64_t processed_counter = 0;
    void enqueue_bulk(std::vector<data_type>&& new_data)
    {
        //TODO FIX
    }
    void blocking_enqueue(data_type&& new_hit)
    {
        //if(new_hit.tot() < 0)
        //    std::cout << "Enqueued last hit";
        ++processed_counter;
        if(processed_counter % 10000 == 0)
            while(queue_.size_approx() > MAX_Q_LEN)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            }
        queue_.enqueue(std::move(new_hit));
    }
    bool blocking_dequeue(data_type & new_hit)
    {
        while(!queue_.try_dequeue(new_hit)){
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
        return true;
    }
    uint32_t approx_size()
    {
        return queue_.size_approx();
    }



};
