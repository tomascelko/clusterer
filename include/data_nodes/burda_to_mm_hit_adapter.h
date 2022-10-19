#include <algorithm>
#include <queue>
#include "../data_flow/dataflow_package.h"
#include "../data_structs/mm_hit.h"
#include "../data_structs/mm_hit_tot.h"
#include "../data_structs/burda_hit.h"
#include "../utils.h"
#include "../data_structs/calibration.h"
#include <type_traits>
#include <cassert>

template <typename mm_hit_type>
class burda_to_mm_hit_adapter : public i_simple_consumer<burda_hit>, public i_simple_producer<mm_hit_type>
{
    bool calibrate_;
    uint16_t chip_width_;
    uint16_t chip_height_;
    const double fast_clock_dt = 1.5625; //nanoseconds
    const double slow_clock_dt = 25.;
    std::unique_ptr<calibration> calibrator_;
    private:
    mm_hit_type convert_hit(const burda_hit & in_hit)
    {
        double toa = slow_clock_dt * in_hit.toa() - fast_clock_dt * in_hit.fast_toa(); 
        short y = in_hit.linear_coord() / chip_width_;
        short x = in_hit.linear_coord() % chip_width_;
        if(calibrate_)
            return mm_hit_type(x, y, toa, calibrator_->compute_energy(x, y, (in_hit.tot())));
        else
            return mm_hit_type(x, y, toa, in_hit.tot());
        
    }

    public:
    burda_to_mm_hit_adapter(const coord& chip_size) :
    chip_height_(chip_size.x()),
    chip_width_(chip_size.y()),
    calibrate_(false)
    {
        assert((std::is_same<mm_hit_type, mm_hit_tot>::value));
    }

    burda_to_mm_hit_adapter(const coord& chip_size, calibration && calib) :
    chip_height_(chip_size.x()),
    chip_width_(chip_size.y()),
    calibrate_(true),
    calibrator_(std::make_unique<calibration>(std::move(calib)))
    {
        assert((std::is_same<mm_hit_type, mm_hit>::value));
    }


    virtual void start() override
    {
        burda_hit hit;
        reader_.read(hit);
        while(hit.is_valid())
        {
            mm_hit_type converted_hit = convert_hit(hit);
            this->writer_.write(convert_hit(hit));
            if(converted_hit.e() < 0)
            {
                std::cout << "negative energy found" << std::endl;
            }
            if(converted_hit.e() > 100000)
            {
                std::cout << "above 100000 energy found" << std::endl;
            }
            reader_.read(hit);
        }
        this->writer_.write(mm_hit_type::end_token());
        this->writer_.flush();
        std::cout << "ADAPTER ENDED ---------------------" << std::endl;
    }
    virtual ~burda_to_mm_hit_adapter() = default;
};