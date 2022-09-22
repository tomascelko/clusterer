
#include "abstract_pipe.h"
#include "readerwriterqueue.h"
#include "concurrentqueue.h"
#pragma once
template <typename data_type>
class data_block
{
    std::vector<data_type> data_block_;
    int32_t to_delete_index_;
    uint32_t max_capacity_;
    public:
    data_block(uint64_t size) :
    max_capacity_(size),
    to_delete_index_(0)
    {
        data_block_.reserve(size);
    }
    bool try_add_hit(data_type && data)
    {
        data_block_.emplace_back(data);
        return data_block_.size() < max_capacity_;
    }
    bool try_remove_hit(data_type & hit)
    {
        if (to_delete_index_ == data_block_.size())
        {
            return false;
        }
        hit = std::move(data_block_[to_delete_index_]);
        //TODO try with erase instead
        ++to_delete_index_;
        /*if(to_delete_index_ == data_block_.size())
        {
            data_block_.clear();
            to_delete_index_ = 0;
        }*/
        return true;
    }
    void clear()
    {
        data_block_.clear();
    }
    
};


template <typename data_type, template<typename> typename queue_type = moodycamel::ConcurrentQueue>
class pipe : public abstract_pipe
{
    //todo use simple one producer one consumer queue and create multi pipe : public pipe with this multi concurrent queue
private:
    const uint32_t CHECK_FULL_PIPE_INTERVAL = 500;
    queue_type<data_block<data_type>> queue_;
    uint32_t id_;
    void emplace_impl(data_block<data_type> && block, moodycamel::ReaderWriterQueue<data_block<data_type>> & queue)
    {
        queue_.emplace(block);
    }
    void emplace_impl(data_block<data_type> && block, moodycamel::ConcurrentQueue<data_block<data_type>> & queue)
    {
        queue_.enqueue(block);
    }
public: 
    static constexpr uint32_t MAX_Q_LEN = 2 << 18;
    pipe(uint32_t id) :
    id_(id),
    queue_(MAX_Q_LEN){}
    uint64_t processed_counter = 1;
    //not thread safe TODO move in block and out block to writer and reader so we can use the buffering with multiple writers
    //WILL be called from multiple threads on a same pipe
    void blocking_enqueue(data_block<data_type> && new_block)
    {
        //if(new_hit.tot() < 0)
        //    std::cout << "Enqueued last hit";
        ++processed_counter; //potentially rally condition
        if(processed_counter % CHECK_FULL_PIPE_INTERVAL == 0)
            while(is_full())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        emplace_impl(std::move(new_block), queue_); //TODO or call enqueue when using multi queue
        
    }
    //not thread safe TODO move in block and out block to writer and reader so we can use the buffering with multiple writers
    //WILL NOT be called from multiple threads on a same pipe
    bool blocking_dequeue(data_block<data_type> & out_block)
    {
        while(!queue_.try_dequeue(out_block))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return true;
    }
    uint32_t approx_size()
    {
        return queue_.size_approx();
    }
    bool is_full()
    {
        return queue_.size_approx() > MAX_Q_LEN;
    }
    



};
