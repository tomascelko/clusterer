#include "../../data_flow/dataflow_package.h"

template <typename hit_type>
class clustering_validator : public i_data_consumer<cluster<hit_type>>
{
    //std::ostream* out_stream_;
    multi_pipe_reader<cluster<hit_type>> reader_;
    public:
    clustering_validator() //:
    //out_stream_()
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
    std::deque<cluster<hit_type>> clusters_0;
    std::deque<cluster<hit_type>> clusters_1;
    const double EPSILON_FTOA = 0.01;
    uint64_t intersection_size = 0;
    uint64_t union_size = 0;
    std::deque<cluster<hit_type>>& get_clusters(bool pick_first)
    {
        if(pick_first)
            return clusters_0;
        return clusters_1;
    }
    void compare_clusters()
    {
        bool is_first_older = clusters_0[0].first_toa() < clusters_1[1].first_toa();
        std::deque<cluster<hit_type>>& oldest_cls = get_clusters(is_first_older);
        cluster<hit_type>& oldest_cl = oldest_cls[0];
        std::deque<cluster<hit_type>>& to_compare_cls = get_clusters(!is_first_older);
        ++union_size;
        bool matched = false;
        for (auto it = to_compare_cls.begin(); it != to_compare_cls.end(); ++it)
        {
            if(std::abs(it->first_toa() - oldest_cl.first_toa()) > EPSILON_FTOA)
                break;
            if(oldest_cl.approx_equals(*it))
            {
                to_compare_cls.erase(it);
                ++intersection_size;
                matched = true;
                break;
            }
        }
        oldest_cls.pop_front();
        
    
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
        //out_stream_ << "Intersection: " << intersection_size << std::endl;
        //out_stream_ << "Union: " << union_size << std::endl; 
        union_size += clusters_0.size() + clusters_1.size();

        result_ = std::to_string(intersection_size / (double) union_size);
        
        std::cout << "VALIDATOR ENDED ----------------" << std::endl;
    }
    std::string result_;
    std::string result() override
    {
        return result_;
    }
    virtual ~clustering_validator() = default;
};