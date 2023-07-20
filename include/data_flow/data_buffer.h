#include <vector>
#include <memory>
#include <iostream>
#pragma once
// TODO handle termination condition
template <typename data_type>
class data_buffer
{

    std::vector<data_type> data_;

    uint32_t max_size_ = 0x1000000;

public:
    data_buffer(uint32_t max_size) : max_size_(max_size)
    {
    }

    enum class state
    {
        full,
        processing,

    };
    void set_state(state new_state)
    {
        state_ = new_state;
    }

    std::vector<data_type> &data()
    {
        return data_;
    }
    void reserve(uint32_t size)
    {
        data_.reserve(size);
    }
    void clear()
    {
        data_.clear();
        state_ = state::processing;
    }
    void emplace_back(data_type &&hit)
    {
        data_.emplace_back(hit);
        if (data_.size() == max_size_)
        {
            state_ = state::full;
        }
    }

private:
    state state_ = state::processing;

public:
    state state()
    {
        return state_;
    }
    const data_type &operator[](uint32_t index) const
    {
        return data_[index];
    }
    data_type &operator[](uint32_t index)
    {
        return data_[index];
    }
    size_t size() const
    {
        return data_.size();
    }
};