
#include "abstract_pipe.h"
#include "../../moodycamel/readerwriterqueue.h"
#include "../../moodycamel/concurrentqueue.h"
#pragma once

//wraps individual pieces of data into a block for better transfer speed
template <typename data_type>
class data_block
{
    std::vector<data_type> data_block_;
    int32_t to_delete_index_;
    uint64_t byte_capacity_;
    uint64_t byte_size_ = 0;
    bool fixed_size_;

public:
    bool is_fixed_size() const
    {
        return fixed_size_;
    }

    uint64_t byte_size()
    {
        return byte_size_;
    }
    uint64_t size()
    {
        return data_block_.size();
    }
    static constexpr uint32_t max_block_byte_size()
    {
        return 2 << 17;
    }
    static constexpr double closing_time()
    {
        return 200.;
    }
    data_block(uint64_t avg_data_size = data_type::avg_size()) : byte_capacity_(data_block<data_type>::max_block_byte_size()),
                                                                 to_delete_index_(0),
                                                                 fixed_size_(true)
    {
        data_block_.reserve(2 * data_block<data_type>::max_block_byte_size() / avg_data_size);
    }

    bool try_add_hit(data_type &&data)
    {
        data_block_.emplace_back(data);
        byte_size_ += data.size();
        return byte_size_ < max_block_byte_size();
    }
    bool try_remove_hit(data_type &hit)
    {
        if (to_delete_index_ == data_block_.size())
        {
            return false;
        }
        hit = std::move(data_block_[to_delete_index_]);
        byte_size_ -= hit.size();
        
        ++to_delete_index_;
        return true;
    }
    bool can_peek()
    {
        return to_delete_index_ != data_block_.size();
    }
    const data_type &peek()
    {
        return data_block_[to_delete_index_];
    }
    void clear(uint64_t avg_data_size = data_type::avg_size())
    {
        byte_size_ = 0;
        data_block_.clear();
        data_block_.reserve(2 * data_block<data_type>::max_block_byte_size() / avg_data_size);
    }
};

//a wrapper around the queue implemented by moodycamel
template <typename data_type, template <typename> typename queue_type = moodycamel::ConcurrentQueue>
class default_pipe : public abstract_pipe
{
private:
    const uint32_t CHECK_FULL_PIPE_INTERVAL = 500;
    const uint32_t HISTORY_LENGTH = 300;
    const uint64_t MAX_QUEUE_SIZE = 2 << 28;
    const uint64_t FULL_QUEUE_SLEEP_TIME = 100; // in miliseconds
    queue_type<data_block<data_type>> queue_;
    std::atomic<uint64_t> bytes_used_ = 0;
    std::string name_;
    uint64_t total_bytes_processed_ = 0;
    i_data_node *producer_;
    i_data_node *consumer_;
    //two variants of emplace method allow for simple switching between implementations of the queue
    //as of our testing no significant performance difference was observed between these implementations
    void emplace_impl(data_block<data_type> &&block, moodycamel::ReaderWriterQueue<data_block<data_type>> &queue)
    {
        queue_.emplace(block);
    }
    void emplace_impl(data_block<data_type> &&block, moodycamel::ConcurrentQueue<data_block<data_type>> &queue)
    {
        queue_.enqueue(block);
    }

public:
    static constexpr uint64_t MAX_Q_LEN = 2ull << 31; // in bytes
    default_pipe(const std::string &name, i_data_node *producer = nullptr, i_data_node *consumer = nullptr) : name_(name),
                                                                                                              producer_(producer),
                                                                                                              consumer_(consumer)
    {
    }
    uint64_t processed_counter = 1;
    i_data_node *producer() override
    {
        return producer_;
    }
    i_data_node *consumer() override
    {
        return consumer_;
    }
    virtual uint64_t bytes_used() override
    {
        return bytes_used_;
    }
    std::string name()
    {
        return name_;
    }
    //estimate of the data size, so the pre-allocation can be done efficiently 
    uint64_t mean_data_size()
    {
        if (processed_counter <= 1)
            return data_type::avg_size();
        return total_bytes_processed_ / processed_counter;
    }
    //important (blocking) method for adding a new data block to queue
    void blocking_enqueue(data_block<data_type> &&new_block)
    {

        processed_counter += new_block.size(); // potentially rally condition
        bytes_used_ += new_block.byte_size();
        total_bytes_processed_ += new_block.byte_size();
        emplace_impl(std::move(new_block), queue_); // TODO or call enqueue when using multi queue
    }
    //important (blocking) method for removing a block from queue
    bool blocking_dequeue(data_block<data_type> &out_block)
    {
        while (!queue_.try_dequeue(out_block))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        bytes_used_ -= out_block.byte_size();
        return true;
    }
    //estimate of the queue size (relatively slow, use with care!)
    uint32_t approx_size()
    {
        return queue_.size_approx();
    }
    virtual ~default_pipe() = default;
};
