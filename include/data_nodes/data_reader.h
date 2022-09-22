#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include "../data_flow/dataflow_package.h"
#include "../utils.h"
template <typename data_type>
class data_reader : public i_data_producer<data_type>
{
    using buffer_type = data_buffer<data_type>;

private:
    std::unique_ptr<buffer_type> buffer_a_;
    std::unique_ptr<buffer_type> buffer_b_;
    uint32_t max_buffer_size_;
    uint64_t total_hits_read_;
    std::unique_ptr<std::ifstream> input_stream_;
    buffer_type* reading_buffer_;
    buffer_type* ready_buffer_ = nullptr;
    pipe_writer<data_type> writer = nullptr;
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
        data_type hit = data_type(input_stream_.get());
        while (hit.is_valid())
        {
            //std::cout << "reading_next" << sstd::endl;
            reading_buffer_->emplace_back(std::move(hit));
            ++processed_count;
            if(reading_buffer_->state() == buffer_type::state::full)
                break;
            hit = data_type(input_stream_.get());
        }
        if(!hit.is_valid())
        {
            reading_buffer_->emplace_back(std::move(hit));
        }
        total_hits_read_ += processed_count;
        //std::cout << total_hits_read_ << std::endl;
        return processed_count == max_buffer_size_;
    }
public:

    data_reader(const std::string& file_name, uint32_t buffer_size) :
    input_stream_(std::move(std::make_unique<std::ifstream>(file_name))),
    max_buffer_size_(buffer_size),
    buffer_a_(std::make_unique<buffer_type>(buffer_size)),
    buffer_b_(std::make_unique<buffer_type>(buffer_size))
    {
        buffer_a_->reserve(buffer_size);
        buffer_b_->reserve(buffer_size);
    }
    virtual void connect_output(pipe<data_type>* out_pipe) override
    {
        writer = pipe_writer<data_type> (out_pipe);
    }
    virtual void start() override
    {
        io_utils::skip_bom(input_stream_.get());
        io_utils::skip_comment_lines(input_stream_.get());
        reading_buffer_ = buffer_a_.get();
        bool should_continue = read_data();
        ready_buffer_ = buffer_a_.get();
        writer.write_bulk(ready_buffer_);
        if(!should_continue)
            return;
        reading_buffer_= buffer_b_.get();
        read_data();
        ready_buffer_ = buffer_b_.get();
        reading_buffer_= buffer_a_.get();
        while(read_next()); //reads while buffer is filled completely everytime
        writer.write_bulk(ready_buffer_);
        writer.flush(); 
        std::cout << "READER ENDED ----------" << std::endl;  
    }
    bool read_next()
    {
        writer.write_bulk(ready_buffer_);
        bool should_continue = read_data();
        swap_buffers();
        return should_continue;
    }

};



