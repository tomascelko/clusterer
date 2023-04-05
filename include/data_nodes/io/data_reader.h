#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include "../../data_flow/dataflow_package.h"
#include "../../utils.h"
#pragma once
template <typename data_type, typename istream_type>
class data_reader : public i_simple_producer<data_type>, public i_controlable_source
{
    protected:
    
    using buffer_type = data_buffer<data_type>;
    const uint32_t SLEEP_DURATION = 100;
    std::unique_ptr<buffer_type> buffer_a_;
    std::unique_ptr<buffer_type> buffer_b_;
    uint32_t max_buffer_size_;
    uint64_t total_hits_read_;
    std::unique_ptr<istream_type> input_stream_;
    buffer_type* reading_buffer_;
    buffer_type* ready_buffer_ = nullptr;
    bool paused_ = false;
    void swap_buffers()
    {
        buffer_type* temp_buffer = reading_buffer_;
        reading_buffer_ = ready_buffer_;
        ready_buffer_ = temp_buffer;
    }
    bool read_data()
    {
        reading_buffer_->set_state(buffer_type::state::processing);
        uint64_t processed_count = 0;
        data_type data;
        *input_stream_ >> data;
        while (data.is_valid())
        {
            //std::cout << "reading_next" << sstd::endl;
            reading_buffer_->emplace_back(std::move(data));
            ++processed_count;
            if(reading_buffer_->state() == buffer_type::state::full)
                break;
            *input_stream_ >> data;
        }
        if(!data.is_valid())
        {
            reading_buffer_->emplace_back(std::move(data));
        }
        total_hits_read_ += processed_count;
        //std::cout << total_hits_read_ << std::endl;
        return processed_count == max_buffer_size_;
    }
public:

    data_reader(const std::string& file_name, uint32_t buffer_size) :
    input_stream_(std::move(std::make_unique<istream_type>(file_name))),
    max_buffer_size_(buffer_size),
    buffer_a_(std::make_unique<buffer_type>(buffer_size)),
    buffer_b_(std::make_unique<buffer_type>(buffer_size))
    {
        buffer_a_->reserve(buffer_size);
        buffer_b_->reserve(buffer_size);
    }
    virtual void pause_production() override
    {
        paused_ = true;
    }
    virtual void continue_production() override
    {
        paused_ = false;
    }
    std::string name() override
    {
        return "mm_reader";
    }
    virtual void start() override
    {
        auto istream_optional = dynamic_cast<std::istream*>(input_stream_.get()); 
        if (istream_optional != nullptr)
        {
            io_utils::skip_bom(*istream_optional);
            io_utils::skip_comment_lines(*istream_optional);
        }
        reading_buffer_ = buffer_a_.get();
        bool should_continue = read_data();
        ready_buffer_ = buffer_a_.get();
        this->writer_.write_bulk(ready_buffer_);
        if(!should_continue)
            return;
        reading_buffer_= buffer_b_.get();
        read_data();
        ready_buffer_ = buffer_b_.get();
        reading_buffer_= buffer_a_.get();
        while(read_next()) //reads while buffer is filled completely everytime
        {
            while(paused_)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_DURATION));
            }
        }
        this->writer_.write_bulk(ready_buffer_);
        this->writer_.flush(); 
        std::cout << "READER ENDED ----------" << std::endl;  
    }
    bool read_next()
    {
        this->writer_.write_bulk(ready_buffer_);
        bool should_continue = read_data();
        swap_buffers();
        return should_continue;
    }
    virtual ~data_reader() = default;

};



