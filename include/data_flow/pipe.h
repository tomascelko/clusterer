
#include "abstract_pipe.h"
#include "readerwriterqueue.h"
#include "concurrentqueue.h"
#pragma once
template <typename data_type>
class data_block
{
    //try with vector
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

template <typename data_type>
class pipe : public abstract_pipe
{
    //todo use simple one producer one consumer queue and create multi pipe : public pipe with this multi concurrent queue
private:
    moodycamel::ReaderWriterQueue<data_block<data_type>> queue_;
    data_block<data_type> in_block_;
    data_block<data_type> out_block_;
    uint32_t id_;
public: 
    static constexpr uint32_t MAX_Q_LEN = 2 << 18;
    pipe(uint32_t id) :
    id_(id),
    in_block_(2 << 7),
    out_block_(2 << 7),
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
        if(!in_block_.try_add_hit(std::move(new_hit)))
        {
            ++processed_counter;
            if(processed_counter % 100 == 0)
                while(queue_.size_approx() > MAX_Q_LEN)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            flush();
        }
    }
    bool blocking_dequeue(data_type & new_hit)
    {
        while(!out_block_.try_remove_hit(new_hit))
        {
            while(!queue_.try_dequeue(out_block_)){
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        }
        return true;
    }
    uint32_t approx_size()
    {
        return queue_.size_approx();
    }
    void flush()
    {
            queue_.emplace(std::move(in_block_));
            in_block_.clear(); 
    }



};
