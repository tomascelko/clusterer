#include "data_reader.h"

#pragma once
template <typename data_type, typename istream_type>
class repeating_data_reader : public data_reader<data_type, istream_type>
{
    static constexpr uint32_t REPETITION_COUNT = 40;
    double freq_multiplier_;
    using buffer_type = typename data_reader<data_type, istream_type>::buffer_type;
    std::unique_ptr<buffer_type> buffer_;
    void store_buffer(std::unique_ptr<buffer_type> && buffer)
    {
        buffer_ = std::move(buffer);
    }
    void init_read_data()
    {
        io_utils::skip_bom(*this->input_stream_.get());
        io_utils::skip_comment_lines(*this->input_stream_.get());
        this->reading_buffer_ = this->buffer_a_.get();
        bool should_continue = this->read_data();
        store_buffer(std::move(this->buffer_a_));

    }
    data_type offset_hit(const data_type & hit, uint64_t offset)
    {
        return data_type{hit.linear_coord(), static_cast<uint64_t>(std::round(((hit.toa() + offset) * (1. / freq_multiplier_)))), hit.fast_toa(), hit.tot()};
    }
    public:
    repeating_data_reader(const std::string & filename, uint32_t buffer_size, double freq_multiplier = 1) :
    data_reader<data_type, istream_type>(filename, buffer_size),
    freq_multiplier_(freq_multiplier)
    {}
    std::string name() override
    {
        return "recurring_adjustable_data_reader";
    }
    virtual void start() override
    {
        init_read_data();
        buffer_type & rep_buffer = (*buffer_);
        uint64_t toa_offset = (rep_buffer[rep_buffer.size() - 1].toa() - rep_buffer[0].toa()) * 2;

        for(uint32_t rep_index = 0; rep_index < REPETITION_COUNT; ++rep_index)
        {
            for(uint32_t hit_index = 0; hit_index < buffer_->size(); ++hit_index)
            {
                this->writer_.write(offset_hit(rep_buffer[hit_index], toa_offset * rep_index));
                while(this->paused_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(this->SLEEP_DURATION));
                }
            }
        
        }
        this->writer_.write(data_type::end_token());
        this->writer_.flush();
        std::cout << "REPEATING READER ENDED ----------" << std::endl;  
    }
};