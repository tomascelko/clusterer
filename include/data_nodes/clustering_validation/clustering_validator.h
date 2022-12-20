#include "../../data_flow/dataflow_package.h"

template <typename hit_type>
class clustering_validator : public i_data_consumer<cluster<hit_type>>
{
    std::ostream& out_stream_;
    multi_cluster_pipe_reader<hit_type> reader_;
    public:
    clustering_validator(std::ostream& print_stream) :
    out_stream_((print_stream))
    {
        //out_stream_ = std::move(std::make_unique<std::ostream>(print_stream));
    }
    std::string name() override
    {
        return "clustering IoU validator";
    }
    void connect_input(default_pipe<cluster<hit_type>>* input_pipe) override
    {
        reader_.add_pipe(input_pipe);
    }
    std::vector<cluster<hit_type>> clusters_0;
    std::vector<cluster<hit_type>> clusters_1;
    const double EPSILON_FTOA = 0.01;
    uint64_t intersection_size = 0;
    uint64_t union_size = 0;
    std::vector<cluster<hit_type>> get_clusters(bool pick_first)
    {
        if(pick_first)
            return clusters_0;
        return clusters_1;
    }
    void compare_clusters()
    {
        bool is_first_older = clusters_0[0].first_toa() < clusters_1[1].first_toa();
        auto oldest_cls = get_clusters(is_first_older);
        auto oldest_cl = oldest_cls[0];
        auto to_compare_cls = get_clusters(!is_first_older);
        ++union_size;
        for (auto it = to_compare_cls.begin(); it != to_compare_cls.end(); ++it)
        {
            if(std::abs(it->first_toa() - oldest_cl.first_toa()) > EPSILON_FTOA)
                break;
            if(oldest_cl.approx_equals(*it))
            {
                to_compare_cls.erase(it);
                ++intersection_size;
                break;
            }
        }
        oldest_cls.erase(oldest_cls.begin());
        
    
    }
    virtual void start() override
    {
        cluster<hit_type> cl;
        uint64_t processed = 0;
        this->reader_.read(cl);

        while(cl.is_valid())
        {
            if(this->reader_.last_read_pipe() == 0)
                clusters_0.push_back(cl);
            else
                clusters_1.push_back(cl);
            while(clusters_0.size() > 0 && clusters_1.size() > 0 && 
                std::abs(clusters_0.back().first_toa() - clusters_1.back().first_toa()) > EPSILON_FTOA)
                compare_clusters();
            ++processed; 
            this->reader_.read(cl);

        }
        out_stream_ << "Intersection: " << intersection_size << std::endl;
        out_stream_ << "Union: " << union_size << std::endl; 
        out_stream_.flush();
        std::cout << "PRINTER ENDED ----------------" << std::endl;
    }

    virtual ~clustering_validator() = default;
};