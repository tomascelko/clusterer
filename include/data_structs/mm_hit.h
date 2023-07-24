#pragma once
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include "../utils.h"
//suring conversion, the burda_hit is often converted to mm_hit
//which is easier to use for analysis
class mm_hit
{
    //spatial pixel coordinates (x,y)
    coord coord_;
    //time of arrival in ns
    double toa_;
    //deposited energy in keV
    double e_;

public:
    mm_hit(short x, short y, double toa, double e) : coord_(x, y),
                                                     toa_(toa),
                                                     e_(e)
    {
    }
    constexpr static uint64_t avg_size()
    {
        return 2*sizeof(double) + sizeof(coord);
    }
    static mm_hit end_token()
    {
        mm_hit end_token(0, 0, -1, 0);
        return end_token;
    }
    // TODO try to get rid of default constructor -> and prevent copying
    mm_hit()
    {
    }

    bool is_valid()
    {
        return toa_ >= 0;
    }

    const coord &coordinates() const
    {
        return coord_;
    }

    uint64_t size()
    {
        return mm_hit::avg_size();
    }
    short x() const
    {
        return coord_.x_;
    }
    short y() const
    {
        return coord_.y_;
    }
    double time() const
    {
        return toa();
    }
    double toa() const
    {
        return toa_;
    }
    double e() const
    {
        return e_;
    }
    bool approx_equals(const mm_hit &other)
    {
        const double epsilon = 0.01;
        return (x() == other.x() && y() == other.y() && std::abs(toa() - other.toa()) < epsilon && std::abs(e() - other.e() < epsilon));
    }
};
//hit serialization
template <typename stream_type>
stream_type &operator<<(stream_type &os, const mm_hit &hit)
{
    os << hit.x() << " " << hit.y() << " " << double_to_str(hit.toa()) << " ";
    os << double_to_str(hit.e(), 2) << "\n";

    return os;
}
//hit deserialization
template <typename stream_type>
stream_type &operator>>(stream_type &istream, mm_hit &hit)
{
    short x, y;
    double toa, e;
    istream >> x >> y >> toa >> e;
    hit = mm_hit(x, y, toa, e);

    return istream;
}
