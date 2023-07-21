#include <algorithm>
#include <queue>
#include "../../data_flow/dataflow_package.h"
#include "../../data_structs/mm_hit.h"
#include "../../data_structs/mm_hit_tot.h"
#include "../../data_structs/burda_hit.h"
#include "../../utils.h"
#include "../../data_structs/calibration.h"
#include "../../devices/current_device.h"
#include <type_traits>
#include <cassert>
//converts the burda hit to mm_hit
//computes energy, and time in nanoseconds
template <typename mm_hit_type>
class burda_to_mm_hit_adapter : public i_simple_consumer<burda_hit>, public i_multi_producer<mm_hit_type>
{
    bool calibrate_;
    uint16_t chip_width_;
    uint16_t chip_height_;
    uint16_t last_x;
    uint16_t last_y;
    std::unique_ptr<calibration> calibrator_;
    uint64_t repetition_counter = 0;
    //an option to ignore repetetive hits (indication of chip failure)
    bool ignore_repeating_pixel = false;
    uint16_t max_faulty_repetition_count = 10;

public:
    const double fast_clock_dt = 1.5625; // nanoseconds
    const double slow_clock_dt = 25.;
    //compute energy in keV and toa in nanoseconds
    mm_hit_type convert_hit(const burda_hit &in_hit)
    {
        double toa = in_hit.toa();
        short y = in_hit.linear_coord() / chip_width_;
        short x = in_hit.linear_coord() % chip_width_;

        if (calibrate_)
            return mm_hit_type(x, y, toa, calibrator_->compute_energy(x, y, (in_hit.tot())));
        else
            return mm_hit_type(x, y, toa, in_hit.tot());
    }
    //inverse conversion of hits (mm hit to burda hit),
    //required for frequency scaling, to locate clustered hits
    burda_hit reverse_incomplete_convert_hit(const mm_hit_type &in_hit)
    {
        int64_t slow_ticks = std::ceil(in_hit.toa() / slow_clock_dt);
        short fast_ticks = std::round((slow_ticks * slow_clock_dt - in_hit.toa()) / fast_clock_dt);
        uint16_t linear_coord = in_hit.y() * chip_width_ + in_hit.x();
        return burda_hit(linear_coord, slow_ticks, fast_ticks, 1);
    }

    using split_descriptor_type = split_descriptor<mm_hit_type>;

    burda_to_mm_hit_adapter(node_descriptor<burda_hit, mm_hit_type> *node_descriptor) : chip_height_(current_chip::chip_type::size_x()),
                                                                                        chip_width_(current_chip::chip_type::size_y()),
                                                                                        calibrate_(false),
                                                                                        i_multi_producer<mm_hit_type>(node_descriptor->split_descr)
    {
        assert((std::is_same<mm_hit_type, mm_hit_tot>::value));
    }

    burda_to_mm_hit_adapter() : chip_height_(current_chip::chip_type::size_x()),
                                chip_width_(current_chip::chip_type::size_y()),
                                calibrate_(false)
    {
        assert((std::is_same<mm_hit_type, mm_hit_tot>::value));
    }

    burda_to_mm_hit_adapter(node_descriptor<burda_hit, mm_hit_type> *node_descriptor, calibration &&calib) : chip_height_(current_chip::chip_type::size_x()),
                                                                                                             chip_width_(current_chip::chip_type::size_y()),
                                                                                                             calibrate_(true),
                                                                                                             calibrator_(std::make_unique<calibration>(std::move(calib))),
                                                                                                             i_multi_producer<mm_hit_type>(node_descriptor->split_descr)
    {
        assert((std::is_same<mm_hit_type, mm_hit>::value));
    }

    burda_to_mm_hit_adapter(calibration &&calib) : chip_height_(current_chip::chip_type::size_x()),
                                                   chip_width_(current_chip::chip_type::size_y()),
                                                   calibrate_(true),
                                                   calibrator_(std::make_unique<calibration>(std::move(calib)))
    {
        assert((std::is_same<mm_hit_type, mm_hit>::value));
    }

    std::string name() override
    {
        return "burda_to_mm_adapter";
    }
    virtual void start() override
    {
        burda_hit hit;
        reader_.read(hit);
        while (hit.is_valid())
        {
            mm_hit_type converted_hit = convert_hit(hit);
            if (converted_hit.x() == last_x && converted_hit.y() == last_y)
                repetition_counter++;
            else
            {
                repetition_counter = 1;
                last_x = converted_hit.x();
                last_y = converted_hit.y();
            }
            if (repetition_counter > max_faulty_repetition_count && ignore_repeating_pixel)
            {
                //optionally ignore suspicious pixels
            }
            else
            {
                this->writer_.write(std::move(converted_hit));
            }
            if (converted_hit.e() < 0)
            {
                // std::cout << "negative energy found" << std::endl;
            }
            if (converted_hit.e() > 100000)
            {
                // std::cout << "above 100000 energy found" << std::endl;
            }
            reader_.read(hit);
        }
        this->writer_.close();
    }
    virtual ~burda_to_mm_hit_adapter() = default;
};