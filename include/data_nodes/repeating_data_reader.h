#include "data_reader.h"

#pragma once
template <typename data_type>
class repeating_data_reader : public data_reader<data_type>
{
    static constexpr uint32_t REPETITION_COUNT = 40;

    using buffer_type = typename data_reader<data_type>::buffer_type;
    std::unique_ptr<buffer_type> buffer_;
    void store_buffer(std::unique_ptr<buffer_type> && buffer)
    {
        buffer_ = std::move(buffer);
    }
    void init_read_data()
    {
        io_utils::skip_bom(this->input_stream_.get());
        io_utils::skip_comment_lines(this->input_stream_.get());
        this->reading_buffer_ = this->buffer_a_.get();
        bool should_continue = this->read_data();
        store_buffer(std::move(this->buffer_a_));

    }
    data_type offset_hit(const data_type & hit, uint64_t offset)
    {
        return data_type{hit.linear_coord(), hit.toa() + offset, hit.fast_toa(), hit.tot()};
    }
    public:
    repeating_data_reader(const std::string & filename, uint32_t buffer_size) :
    data_reader<data_type>(filename, buffer_size)
    {}
    virtual void start() override
    {
        init_read_data();
        buffer_type & rep_buffer = (*buffer_);
        uint64_t toa_offset = (rep_buffer[rep_buffer.size() - 1].toa() - rep_buffer[0].toa()) * 2;

        for(uint32_t rep_index = 0; rep_index < REPETITION_COUNT; ++rep_index)
        {
            for(uint32_t hit_index = 0; hit_index < buffer_->size(); ++hit_index)
            {
                this->writer.write(offset_hit(rep_buffer[hit_index], toa_offset * rep_index));
                while(this->paused_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(this->SLEEP_DURATION));
                }
            }
        
        }
        this->writer.write(data_type::end_token());
        this->writer.flush();
        std::cout << "REPEATING READER ENDED ----------" << std::endl;  
    }
};