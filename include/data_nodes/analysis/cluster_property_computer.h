//computes the cluster properties and serializes them to a file
//as an example, it computes cluster diameter and a timespan
template <template <typename> typename cluster_type, typename hit_type>
class cluster_property_computer : public i_simple_consumer<cluster_type<hit_type>>
{
    const double ENERGY_THRESHOLD = 50;
    const double SQUARE_FULLNES_THRESHOLD = 0.74;
    std::unique_ptr<std::ofstream> output_csv_stream_;
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
    //compute both timespan and cluster diameter
    void process_cluster(const cluster_type<hit_type> &cl)
    {

        auto x_comparer = [](const hit_type &left_coord, const hit_type &right_coord)
        {
            return left_coord.x() < right_coord.x();
        };
        auto y_comparer = [](const hit_type &left_coord, const hit_type &right_coord)
        {
            return left_coord.y() < right_coord.y();
        };
        const auto [min_x, max_x] = std::minmax_element(cl.hits().begin(), cl.hits().end(), x_comparer);
        const auto [min_y, max_y] = std::minmax_element(cl.hits().begin(), cl.hits().end(), y_comparer);
        auto diameter = std::max(max_x->x() - min_x->x() + 1, max_y->y() - min_y->y() + 1);

        *output_csv_stream_ << diameter << "," << cl.last_toa() - cl.first_toa() << ","
                            << "\n";

        
    }

public:
    cluster_property_computer() : output_csv_stream_(std::move(std::make_unique<std::ofstream>("cluster_properties.txt"))),
                                  pixel_matrix_(current_chip::chip_type::size_x(), std::vector<bool>(current_chip::chip_type::size_y(), false))
    {
    }
    std::string name() override
    {
        return "cluster_property_computer";
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

        output_csv_stream_->close();
    }
};