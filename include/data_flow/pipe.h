
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
    uint64_t byte_size_ = 0;
    public:
    uint64_t byte_size()
    {
        return byte_size_;
    }
    static constexpr uint32_t block_size()
    {
        return 2<<7;
    }
    data_block() :
    max_capacity_(data_block<data_type>::block_size()),
    to_delete_index_(0)
    {
        data_block_.reserve(data_block<data_type>::block_size());
    }
    bool try_add_hit(data_type && data)
    {
        data_block_.emplace_back(data);
        byte_size_ += data.size();
        return data_block_.size() < max_capacity_;
    }
    bool try_remove_hit(data_type & hit)
    {
        if (to_delete_index_ == data_block_.size())
        {
            return false;
        }
        hit = std::move(data_block_[to_delete_index_]);
        byte_size_ -= hit.size();
        //TODO try with erase instead
        ++to_delete_index_;
        /*if(to_delete_index_ == data_block_.size())
        {
            data_block_.clear();
            to_delete_index_ = 0;
        }*/
        return true;
    }
    bool can_peek()
    {
        return to_delete_index_ != data_block_.size();
    }
    const data_type & peek()
    {
        return data_block_[to_delete_index_];
    }
    void clear()
    {
        byte_size_ = 0;
        data_block_.clear();
    }
    
};


template <typename data_type, template<typename> typename queue_type = moodycamel::ConcurrentQueue>
class default_pipe : public abstract_pipe
{
    //todo use simple one producer one consumer queue and create multi default_pipe : public default_pipe with this multi concurrent queue
private:
    const uint32_t CHECK_FULL_PIPE_INTERVAL = 500;
    queue_type<data_block<data_type>> queue_;
    std::atomic<uint64_t> bytes_used_ = 0;
    std::string name_;
    void emplace_impl(data_block<data_type> && block, moodycamel::ReaderWriterQueue<data_block<data_type>> & queue)
    {
        queue_.emplace(block);
    }
    void emplace_impl(data_block<data_type> && block, moodycamel::ConcurrentQueue<data_block<data_type>> & queue)
    {
        queue_.enqueue(block);
    }
public: 
    static constexpr uint64_t MAX_Q_LEN = 2ull << 31; //in bytes
    default_pipe(const std::string & name) :
    name_(name){
    }
    uint64_t processed_counter = 1;
    //not thread safe TODO move in block and out block to writer and reader so we can use the buffering with multiple writers
    //WILL be called from multiple threads on a same pipe
    virtual uint64_t bytes_used() override
    {
        return bytes_used_;
    }
    std::string name()
    {
        return name_;
    }
    void blocking_enqueue(data_block<data_type> && new_block)
    {
        ++processed_counter; //potentially rally condition
        bytes_used_ += new_block.byte_size();   
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
        bytes_used_ -= out_block.byte_size();
        return true;
    }
    uint32_t approx_size()
    {
        return queue_.size_approx();
    }
    virtual ~default_pipe() = default;



};
