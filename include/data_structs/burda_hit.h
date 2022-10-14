#include <memory>
#include <vector>
#include <iostream>
#include "../utils.h"
#pragma once
class burda_hit
{
    uint16_t linear_coord_;
    int64_t toa_;
    short fast_toa_;
    int16_t tot_;  //can be zero because of chip error, so we set invalid value to -1
public:
//TODO create producer of burda hits (burda reader) instead of burda hit  class reading from stream
    burda_hit(uint16_t linear_coord, uint64_t toa, short fast_toa, int16_t tot):
    linear_coord_(linear_coord),
    toa_(toa),
    fast_toa_(fast_toa),
    tot_(tot){}
    burda_hit(std::istream * in_stream)
    {
        toa_ = -1;
        //TODO call skip comment before each read (now causes error)
        io_utils::skip_comment_lines(in_stream);
        if(in_stream->peek() == EOF)
            return;
        *in_stream >> linear_coord_;
            (*in_stream) >> toa_ >> fast_toa_ >> tot_;

    }
    static constexpr uint64_t avg_size()
    {
        return (sizeof(decltype(linear_coord_)) + sizeof(decltype(toa_)) +  sizeof(decltype(fast_toa_)) + sizeof(decltype(tot_)));
    }
    static burda_hit end_token()
    {
        burda_hit end_token(0,-1,0,0);
        return end_token;
    }
    burda_hit()
    {}
    bool is_valid()
    {
        return toa_ >= 0;
    }
    uint16_t linear_coord() const
    {
        return linear_coord_;
    }
    uint64_t toa() const
    {
        return toa_;
    }
    short fast_toa() const
    {
        return fast_toa_;
    }
    int16_t tot() const
    {
        return tot_;
    }
    uint32_t size() const
    {
        return avg_size();
    }
};
std::ostream& operator<<(std::ostream& os, const burda_hit& hit)
{
    os << hit.linear_coord() << " " << hit.toa() << " " << hit.fast_toa() << " " << hit.tot()  ;
    return os;
}
