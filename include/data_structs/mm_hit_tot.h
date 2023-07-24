#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include "../utils.h"
#pragma once
// uncalibrated
class mm_hit_tot
{
    coord coord_;
    double toa_, tot_;

public:
    mm_hit_tot(short x, short y, double toa, double tot) : coord_(x, y),
                                                           toa_(toa),
                                                           tot_(tot)
    {
    }
    // e_(e){}
    // TODO try to get rid of default constructor -> and prevent copying
    mm_hit_tot() : coord_(0, 0),
                   toa_(0.),
                   tot_(0.)
    {
    }
    // e_(0.){}
    bool is_valid()
    {
        return tot_ >= 0.;
    }
    /*double e() const
    {
        return e_;
    }*/
    const coord &coordinates() const
    {
        return coord_;
    }
    short x() const
    {
        return coord_.x_;
    }
    short y() const
    {
        return coord_.y_;
    }
    double toa() const
    {
        return toa_;
    }
    double tot() const
    {
        return tot_;
    }
};
std::ostream &operator<<(std::ostream &os, const mm_hit_tot &hit)
{
    os << hit.x() << " " << hit.y() << " " << std::fixed << std::setprecision(6) << hit.toa() << " ";
    os << hit.tot() << std::endl;
    return os;
}