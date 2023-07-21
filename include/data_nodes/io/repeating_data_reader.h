#include "data_reader.h"
#include "../../utils.h"
#pragma once
//reader capable of offsetting hits and modifying frequency of data stream

template <typename data_type, typename istream_type>
class repeating_data_reader : public data_reader<data_type, istream_type>
{
    //a reference to the datatype
    //instead of sorting data types, we sort only references (for quick searching)
    //this way we preserve the unsortedness in the real data
    struct data_type_wrapper_reference
    {
        data_type data;
        uint64_t index;
        data_type_wrapper_reference(const data_type &data_field, uint64_t index) : data(data_field),
                                                                                   index(index) {}
        bool operator<(const data_type_wrapper_reference &other) const
        {
            return data < other.data;
        }
        bool operator==(const data_type_wrapper_reference &other) const
        {
            return data == other.data;
        }
    };
    uint32_t repetition_count_ = 1;
    double freq_multiplier_;
    int64_t buffer_size_;
    using buffer_type = typename data_reader<data_type, istream_type>::buffer_type;
    std::unique_ptr<buffer_type> buffer_;

    std::vector<data_type> rep_buffer_;
    //auxiliary fileds used for frequency scaling of the hits
    std::vector<data_type_wrapper_reference> rep_buffer_sorted_;
    std::unique_ptr<burda_to_mm_hit_adapter<mm_hit>> calibrator_;
    std::unique_ptr<pixel_list_clusterer<cluster>> clusterer_;

    void store_buffer(std::unique_ptr<buffer_type> &&buffer)
    {
        buffer_ = std::move(buffer);
    }
    
    void offset_matching_hit(const mm_hit &hit, double new_time)
    {
        auto reverse_converted_hit = calibrator_->reverse_incomplete_convert_hit(hit);
        auto it = binary_find(rep_buffer_sorted_, data_type_wrapper_reference{reverse_converted_hit, 0});
        if (it == rep_buffer_sorted_.end())
            throw std::invalid_argument("error in backward conversion of hits");
        int64_t slow_ticks = std::ceil(new_time / calibrator_->slow_clock_dt);
        short fast_ticks = std::round((slow_ticks * calibrator_->slow_clock_dt - new_time) / calibrator_->fast_clock_dt);
        rep_buffer_[it->index].update_time(slow_ticks, fast_ticks);
    }

    void squeeze_hits_in_buffer(std::vector<cluster<mm_hit>> &clusters)
    {
        //loop over clusters
        for (auto &cluster : clusters)
        {
            cluster.temporal_sort();
            auto first_hit = cluster.hits()[0];
            double new_timestamp = (1 / freq_multiplier_) * first_hit.toa();
            for (auto &hit : cluster.hits())
            {
                //finds matching burda hit in the buffer
                //and scales it such that the cluster timespan is preserved
                offset_matching_hit(hit, new_timestamp + hit.time() - first_hit.time());
            }
        }
    }
    //runs the clustering on hits, 
    //in order to find first hit in cluster and do the frequency scaling
    //while preserving cluster timespan
    void run_clustering()
    {
        node_args args;
        std::vector<mm_hit> converted_hits;
        converted_hits.reserve(rep_buffer_.size());
        std::transform(rep_buffer_.begin(), rep_buffer_.end(), std::back_inserter(converted_hits),
                       [this](auto hit)
                       { return calibrator_->convert_hit(hit); });
        std::sort(converted_hits.begin(), converted_hits.end(), [](const auto &left_hit, const auto &right_hit)
                  { return left_hit.toa() < right_hit.toa(); });
        std::vector<cluster<mm_hit>> clusters_;
        clusterer_->set_manual_writing();
        for (auto hit : converted_hits)
        {
            clusterer_->process_hit(hit);
            auto new_clusters = clusterer_->retrieve_old_clusters(hit.toa());
            clusters_.insert(clusters_.end(), new_clusters.begin(), new_clusters.end());
        }
        auto new_clusters = clusterer_->retrieve_old_clusters(std::numeric_limits<double>::max());
        clusters_.insert(clusters_.end(), new_clusters.begin(), new_clusters.end());

        for (uint64_t i = 0; i < rep_buffer_.size(); ++i)
        {
            rep_buffer_sorted_.push_back(data_type_wrapper_reference{rep_buffer_[i], i});
        }
        std::sort(rep_buffer_sorted_.begin(), rep_buffer_sorted_.end());
        squeeze_hits_in_buffer(clusters_);
    }
    void init_read_data()
    {
        //read the header
        io_utils::skip_bom(*this->input_stream_.get());
        io_utils::skip_comment_lines(*this->input_stream_.get());

        data_type data;
        *this->input_stream_ >> data;
        uint64_t processed_count = 0;
        if (buffer_size_ > 0)
            rep_buffer_.reserve(buffer_size_);
        //fill the repetition buffer
        while (data.is_valid() && (processed_count < buffer_size_ || buffer_size_ <= 0))
        {
            rep_buffer_.emplace_back(data);
            *this->input_stream_ >> data;
            ++processed_count;
        }
        //if frequency scaling is desired, clustering needs to be done first
        //to preserve cluster timespan
        if (freq_multiplier_ != 1)
            run_clustering();
    }
    //move hit by given offset
    data_type offset_hit(const data_type &hit, uint64_t offset)
    {
        return data_type{hit.linear_coord(), static_cast<int64_t>(std::round(hit.tick_toa() + offset)), hit.fast_toa(), hit.tot()};
    }
    //initialize clustering for frequency scaling
    void initialize_internal_clustering(const std::string &calib_folder, const node_args &args)
    {
        const double EPSILON = 0.000001;
        if (calib_folder != "")
        {
            calibration calib = calibration(calib_folder, current_chip::chip_type::size());
            calibrator_ = std::move(std::make_unique<burda_to_mm_hit_adapter<mm_hit>>(std::move(calib)));
            clusterer_ = std::move(std::make_unique<pixel_list_clusterer<cluster>>(args));
        }
        if (calib_folder == "" && std::abs(freq_multiplier_ - 1) > EPSILON)
            throw std::invalid_argument("No calibration folder provided - it is required for frequency scaling");
    }

public:
    repeating_data_reader(node_descriptor<data_type, data_type> *node_descriptor,
                          const std::string &filename, const node_args &args, const std::string &calib_folder = "") : data_reader<data_type, istream_type>(node_descriptor, filename, args),
                                                                                                                      buffer_size_(args.get_arg<int>(name(), "repetition_size")),
                                                                                                                      freq_multiplier_(args.get_arg<double>(name(), "freq_multiplier")),
                                                                                                                      repetition_count_(args.get_arg<int>(name(), "repetition_count"))
    {
        initialize_internal_clustering(calib_folder, args);
        init_read_data();
    }
    repeating_data_reader(const std::string &filename, const node_args &args,
                          const std::string &calib_folder = "") : data_reader<data_type, istream_type>(filename, args),
                                                                  buffer_size_(args.get_arg<int>(name(), "repetition_size")),
                                                                  freq_multiplier_(args.get_arg<double>(name(), "freq_multiplier")),
                                                                  repetition_count_(args.get_arg<int>(name(), "repetition_count"))
    {
        initialize_internal_clustering(calib_folder, args);
        init_read_data();
    }
    std::string name() override
    {
        return "reader";
    }
    virtual void start() override
    {

        // buffer_type & rep_buffer = (*buffer_);

        uint64_t toa_offset = (rep_buffer_[rep_buffer_.size() - 1].tick_toa() - rep_buffer_[0].tick_toa());
        //repetition of the buffer
        for (uint32_t rep_index = 0; rep_index < repetition_count_; ++rep_index)
        {
            //loop over the buffer
            for (uint32_t hit_index = 0; hit_index < rep_buffer_.size(); ++hit_index)
            {
                this->writer_.write(offset_hit(rep_buffer_[hit_index], toa_offset * rep_index));
                ++this->total_hits_read_;
                this->perform_memory_check();
            }
        }
        this->writer_.close();

    }
};