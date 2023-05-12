#include "data_reader.h"

#pragma once
template <typename data_type, typename istream_type>
class repeating_data_reader : public data_reader<data_type, istream_type>
{
     uint32_t repetition_count_ = 1;
    double freq_multiplier_;
    uint64_t buffer_size_;
    using buffer_type = typename data_reader<data_type, istream_type>::buffer_type;
    std::unique_ptr<buffer_type> buffer_;

    std::vector<data_type> rep_buffer_;
    void store_buffer(std::unique_ptr<buffer_type> && buffer)
    {
        buffer_ = std::move(buffer);
    }
    void init_read_data()
    {
        io_utils::skip_bom(*this->input_stream_.get());
        io_utils::skip_comment_lines(*this->input_stream_.get());
        /*this->reading_buffer_ = this->buffer_a_.get();
        bool should_continue = this->read_data();
        store_buffer(std::move(this->buffer_a_));
        */
        data_type data;
        *this->input_stream_ >> data;
        uint64_t processed_count = 0;
        rep_buffer_.reserve(buffer_size_);
        while (data.is_valid() && processed_count < buffer_size_)
        {
            //std::cout << "reading_next" << sstd::endl;
            rep_buffer_.emplace_back(data);
            *this->input_stream_ >> data;
            ++processed_count;
        }
    }
    data_type offset_hit(const data_type & hit, uint64_t offset)
    {
        return data_type{hit.linear_coord(), static_cast<int64_t>(std::round(((hit.tick_toa() + offset) * (1. / freq_multiplier_)))), hit.fast_toa(), hit.tot()};
    }
    public:
    repeating_data_reader(node_descriptor<data_type, data_type> * node_descriptor,
        const std::string & filename, const node_args & args) :
        data_reader<data_type, istream_type>(node_descriptor, filename, args),
        buffer_size_(args.get_arg<int>(name(), "repetition_size")),
        freq_multiplier_(args.get_arg<double>(name(), "freq_multiplier")),
        repetition_count_(args.get_arg<int>(name(), "repetition_count"))
    {}
    repeating_data_reader(const std::string & filename, const node_args & args) :
        data_reader<data_type, istream_type>(filename, args),
        buffer_size_(args.get_arg<int>(name(), "repetition_size")),
        freq_multiplier_(args.get_arg<double>(name(), "freq_multiplier")),
        repetition_count_(args.get_arg<int>(name(), "repetition_count"))
    {}
    std::string name() override
    {
        return "reader";
    }
    virtual void start() override
    {
        init_read_data();
        //buffer_type & rep_buffer = (*buffer_);
        
        uint64_t toa_offset = (rep_buffer_[rep_buffer_.size() - 1].tick_toa() - rep_buffer_[0].tick_toa()) * 2;

        for(uint32_t rep_index = 0; rep_index < repetition_count_; ++rep_index)
        {
            for(uint32_t hit_index = 0; hit_index < rep_buffer_.size(); ++hit_index)
            {
                this->writer_.write(offset_hit(rep_buffer_[hit_index], toa_offset * rep_index));
                while(this->paused_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(this->sleep_duration_));
                }
            }
        
        }
        this->writer_.close();
        
        //std::cout << "REPEATING READER ENDED ----------" << std::endl;  
    }
};