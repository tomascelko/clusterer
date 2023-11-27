#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include "../../data_flow/dataflow_package.h"
#include "../../utils.h"
#pragma once
//a node which reads the data from a stream using >> operator
template <typename data_type, typename istream_type>
class data_reader : public i_multi_producer<data_type>, public i_controlable_source
{
protected:
    using buffer_type = data_buffer<data_type>;
    //sleep duration after the whole computational graph cannot load more data to RAM
    //after this time, the memory check is repeated
    uint32_t sleep_duration_;
    std::unique_ptr<buffer_type> buffer_a_;
    std::unique_ptr<buffer_type> buffer_b_;
    uint32_t max_buffer_size_;
    //uint64_t total_hits_read_;
    std::unique_ptr<istream_type> input_stream_;
    buffer_type *reading_buffer_;
    buffer_type *ready_buffer_ = nullptr;
    bool paused_ = false;
    dataflow_controller *controller_;
    const uint32_t PIPE_MEMORY_CHECK_INTERVAL = 1000000;
    
    //change open buffer and a full buffer
    void swap_buffers()
    {
        buffer_type *temp_buffer = reading_buffer_;
        reading_buffer_ = ready_buffer_;
        ready_buffer_ = temp_buffer;
    }
    //load the data into a buffer
    bool read_data()
    {
        reading_buffer_->set_state(buffer_type::state::processing);
        uint64_t processed_count = 0;
        data_type data;
        *input_stream_ >> data;
        while (data.is_valid())
        {
            // std::cout << "reading_next" << sstd::endl;
            reading_buffer_->emplace_back(std::move(data));
            ++processed_count;
            if (reading_buffer_->state() == buffer_type::state::full)
                break;
            *input_stream_ >> data;
        }
        if (!data.is_valid())
        {
            reading_buffer_->emplace_back(std::move(data));
        }
        total_hits_processed_ += processed_count;
        // std::cout << total_hits_read_ << std::endl;
        return processed_count == max_buffer_size_;
    }
    
public:
    data_reader(node_descriptor<data_type, data_type> *node_descriptor,
                const std::string &file_name, const node_args &args) : input_stream_(std::move(std::make_unique<istream_type>(file_name))),
                                                                       sleep_duration_(args.get_arg<double>(name(), "sleep_duration_full_memory")),
                                                                       i_multi_producer<data_type>(node_descriptor->split_descr)
    {
        check_input_stream(file_name);
    }
    data_reader(const std::string &file_name, const node_args &args) : input_stream_(std::move(std::make_unique<istream_type>(file_name))),
                                                                       sleep_duration_(args.get_arg<double>(name(), "sleep_duration_full_memory")),
                                                                       i_multi_producer<data_type>(new trivial_split_descriptor<data_type>())
    {
        check_input_stream(file_name);
        
    }

    virtual void pause_production() override
    {
        paused_ = true;
    }

    virtual void continue_production() override
    {
        paused_ = false;
    }
    virtual void store_controller(dataflow_controller *controller) override
    {
        controller_ = controller;
    }
    std::string name() override
    {
        return "reader";
    }
    //check if reading produces too much data to fit to memory
    void perform_memory_check()
    {
        if (this->total_hits_processed_ % PIPE_MEMORY_CHECK_INTERVAL == 0)
        {
            paused_ = controller_->is_full_memory();
            while (paused_)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration_));
                paused_ = controller_->is_full_memory();
            }
        }
    }
    //verify stream is opened
    void check_input_stream(const std::string &filename)
    {
        if (!input_stream_->is_open())
        {
            throw std::invalid_argument("Could not open selected input stream: '" + filename + "'");
        }
    }
    virtual void start() override
    {

        auto istream_optional = dynamic_cast<std::istream *>(input_stream_.get());
        if (istream_optional != nullptr)
        {
            //skip file header
            io_utils::skip_bom(*istream_optional);
            io_utils::skip_comment_lines(*istream_optional);
        }
        data_type data;
        *input_stream_ >> data;
        while (data.is_valid())
        {
            // std::cout << "reading_next" << sstd::endl;
            this->writer_.write(std::move(data));
            *input_stream_ >> data;
            ++total_hits_processed_;
            perform_memory_check();
        }
        this->writer_.close();
        /*reading_buffer_ = buffer_a_.get();
        bool should_continue = read_data();
        //ready_buffer_ = buffer_a_.get();
        for(uint i = 0; i < ready_buffer_[i].size(); ++i)
        {
            this->writer_.write(ready_buffer_[i]);
        }
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
        for(uint i = 0; i < ready_buffer_->size(); ++i)
        {
            this->writer_.write(ready_buffer_[i]);
        }
        this->writer_.flush(); */
        // std::cout << "READER ENDED ----------" << std::endl;
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
