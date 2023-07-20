#include "../../data_flow/dataflow_package.h"
#include "../../utils.h"
#include "../../devices/current_device.h"

template <template <typename> typename cluster_type, typename hit_type, typename stream_type>
class pixel_halo_width_calculator : public i_simple_consumer<cluster_type<hit_type>>
{
    const double ENERGY_THRESHOLD = 50;
    const double SQUARE_FULLNES_THRESHOLD = 0.74;
    std::unique_ptr<stream_type> out_stream_;
    using pixel_matrix_type = std::vector<std::vector<bool>>;
    pixel_matrix_type pixel_matrix_;
    void load_cluster(const cluster_type<hit_type> &cl)
    {
        for (const auto &hit : cl.hits())
        {
            pixel_matrix_[hit.x()][hit.y()] = true;
        }
    }
    void unload_cluster(const cluster_type<hit_type> &cl)
    {

        for (const auto &hit : cl.hits())
        {
            pixel_matrix_[hit.x()][hit.y()] = false;
        }
    }
    uint32_t check_pixel(int32_t x, int32_t y)
    {
        if (x < 0 || y < 0 || x >= current_chip::chip_type::size_x() || y >= current_chip::chip_type::size_y())
            return 0;
        return pixel_matrix_[x][y] ? 1 : 0;
    }
    bool check_square(const hit_type &hit, int32_t radius)
    {
        uint32_t neighbors_count = 0;
        for (int32_t i = -radius; i < radius; ++i)
        {
            neighbors_count += (check_pixel(hit.x() + i, hit.y() - radius) + check_pixel(hit.x() - radius, hit.y() + i) + check_pixel(hit.x() + radius, hit.y() + i) + check_pixel(hit.x() + i, hit.y() + radius));
        }
        neighbors_count += check_pixel(hit.x() + radius, hit.y() + radius);
        double max_neighbors = 4. * (2. * radius + 1) - 4.;
        return neighbors_count / max_neighbors >= SQUARE_FULLNES_THRESHOLD;
    }
    uint32_t find_halo(const hit_type &hit)
    {
        bool should_continue = true;
        int32_t radius = 0;
        while (should_continue)
        {
            ++radius;
            should_continue = check_square(hit, radius);
        }
        return radius - 1;
    }
    void process_cluster(const cluster_type<hit_type> &cl)
    {
        load_cluster(cl);
        for (const auto &hit : cl.hits())
        {
            if (hit.x() == 0 || hit.y() == 0 || hit.x() == current_chip::chip_type::size_x() - 1 || hit.y() == current_chip::chip_type::size_y() - 1)
                return; // cluster is on the border -> we do not want to analyze it
        }
        for (const auto &hit : cl.hits())
        {
            if (hit.e() > ENERGY_THRESHOLD)
            {
                uint32_t halo_size = find_halo(hit);
                *out_stream_ << double_to_str(hit.e(), 2) << "," << halo_size << std::endl;
            }
        }
        unload_cluster(cl);
    }

public:
    pixel_halo_width_calculator(stream_type *stream) : out_stream_(std::move(std::unique_ptr<stream_type>(stream))),
                                                       pixel_matrix_(current_chip::chip_type::size_x(), std::vector<bool>(current_chip::chip_type::size_y(), false))
    {
    }
    std::string name() override
    {
        return "halo_width_calculator";
    }
    void start() override
    {
        cluster_type<hit_type> cl;
        this->reader_.read(cl);
        uint64_t limit = 2 << 25;
        uint64_t iter_counter = 0;
        while (cl.is_valid() && iter_counter < limit)
        {
            process_cluster(cl);
            this->reader_.read(cl);
            ++iter_counter;
        }

        // std::cout << "HALO WIDTH CALCULATOR ENDED ---------------------" << std::endl;
    }
};