#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include "io_utils.h"
#pragma once
class mm_hit
{
    coord coord_;
    double toa_, e_; 
public:
    mm_hit(short x, short y, double toa, double e) :
        coord_(x, y),
        toa_(toa),
        e_(e)
        {}
    static mm_hit end_token()
    {
        mm_hit end_token(0,0,-1, 0);
        return end_token;
    }
        //e_(e){}
    //TODO try to get rid of default constructor -> and prevent copying
    mm_hit() : 
        coord_(0, 0),
        toa_(0.),
        e_(0.)
        {}
        //e_(0.){}
    bool is_valid()
    {
        return toa_ >= 0;
    }
    /*double e() const
    {
        return e_;
    }*/
    const coord& coordinates() const
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
    double e() const
    {
        return e_;
    }

};
std::ostream& operator<<(std::ostream& os, const mm_hit& hit)
{
    os << hit.x() << " " << hit.y() << " " << std::fixed << std::setprecision(6) << hit.toa() << " ";
    os << hit.e() << std::endl;
    return os;
}