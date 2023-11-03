#include "../../data_flow/dataflow_package.h"
#include "window_state.h"
#include "onnxruntime_cxx_api.h"
//base class of trigger, handles automatic untriggering
//and stores time of the last trigger
template <typename hit_type, typename feature_vector_type>
class abstract_window_trigger
{
protected:
    double last_triggered_ = 0;
    double untrigger_time_; // in ns, set to 400 miliseconds (2 windows)
public:
    abstract_window_trigger(double untrigger_time = 400000000) : untrigger_time_(untrigger_time)
    {
    }
    virtual bool trigger(const feature_vector_type &new_state)
    {
        bool triggered = should_trigger(new_state);

        if (triggered)
        {
            last_triggered_ = new_state.last_hit_toa();
        }
        return triggered;
    }
    virtual bool should_trigger(const feature_vector_type &new_state) = 0;
    virtual bool untrigger(const feature_vector_type &new_state)
    {
        return new_state.last_hit_toa() - last_triggered_ > untrigger_time_;
    }
};
//a simple interval trigger, checks if each feature is inside of given intervals
template <typename hit_type>
class interval_trigger : public abstract_window_trigger<hit_type, default_window_feature_vector<hit_type>>
{
    struct interval
    {
        double lower, upper;
        interval(double lower, double upper) : lower(lower), upper(upper) {}
    };
    bool verbose = true;
    uint32_t vector_count_ = 1;
    std::vector<std::map<std::string, interval>> intervals_;
    std::vector<std::string> attribute_names_;
    //methods for parsing the interval trigger file:
    interval parse_interval_bounds(const std::string &interval_str)
    {
        const char INTERVAL_DELIM = ':';
        const std::string ANY_VALUE_STR = "*";
        auto int_delim_pos = interval_str.find(INTERVAL_DELIM);
        auto lower_bound_str = interval_str.substr(0, int_delim_pos);
        auto upper_bound_str = interval_str.substr(int_delim_pos + 1);
        double lower_bound, upper_bound;
        if (lower_bound_str == ANY_VALUE_STR)
            lower_bound = -std::numeric_limits<double>::infinity();
        else
            lower_bound = std::stod(lower_bound_str);
        if (upper_bound_str == ANY_VALUE_STR)
            upper_bound = std::numeric_limits<double>::infinity();
        else
            upper_bound = std::stod(upper_bound_str);
        return interval(lower_bound, upper_bound);
    }

    void load_intervals(const std::string &interval_file)
    {
        const char DELIM = ' ';
        std::ifstream interval_stream(interval_file);
        if (!interval_stream.is_open())
            throw std::invalid_argument("could not load the trigger file: " + interval_file);
        std::string stream_line;
        std::getline(interval_stream, stream_line);
        std::stringstream attribute_ss(stream_line);
        std::string attribute_name;
        while (std::getline(attribute_ss, attribute_name, DELIM))
        {
            attribute_names_.push_back(attribute_name);
        }
        std::stringstream interval_ss;

        std::string interval_str;
        while (std::getline(interval_stream, stream_line))
        {
            std::map<std::string, interval> interval_map;
            interval_ss = std::stringstream(stream_line);
            uint32_t interval_index = 0;
            while (std::getline(interval_ss, interval_str, DELIM))
            {
                auto trimmed_interval_str = interval_str.substr(1, interval_str.size() - 2);
                interval bound_interval = parse_interval_bounds(trimmed_interval_str);
                interval_map.insert(std::make_pair(attribute_names_[interval_index], bound_interval));
                ++interval_index;
            }
            intervals_.emplace_back(interval_map);
        }
    }

public:
    //retrieve node args argument and do interval parsing 
    using state_type = default_window_feature_vector<hit_type>;
    interval_trigger(const std::map<std::string, std::string> &args) : abstract_window_trigger<hit_type, default_window_feature_vector<hit_type>>(std::stod(args.at("trigger_time")))
    {
        std::string interval_file = args.at("trigger_file");
        load_intervals(interval_file);
    }
    //the trigger decision function
    virtual bool should_trigger(const default_window_feature_vector<hit_type> &feat_vector) override
    {
        for (auto &&interval_feature_map : intervals_)
        {
            bool start_trigger = true;
            for (auto &&feature_name : attribute_names_)
            {

                double feature_value = feat_vector.get_scalar(feature_name);
                interval &feat_interval = interval_feature_map.at(feature_name);
                if (std::isinf(-feat_interval.lower) && std::isinf(feat_interval.upper))
                    continue;
                if (feature_value < feat_interval.lower || feature_value > feat_interval.upper)
                {
                    start_trigger = false;
                    break;
                }
            }
            if (start_trigger)
                return true;
        }
        return false;
    }
};
//two templates to handle compatibility with both linux and windows (using SFINAE)
//because of differend datatypes for onnx library
template <typename T>
const T *get_trigger_filename(const std::string &trigger_filename)
{
    return trigger_filename.c_str();
}

template <>
const wchar_t *get_trigger_filename<wchar_t>(const std::string &trigger_filename)
{
    wchar_t *filename_wide = new wchar_t[trigger_filename.size()];
    std::mbstowcs(filename_wide, trigger_filename.c_str(), trigger_filename.size());
    return (const wchar_t *)filename_wide;
}

//the trigger which wraps serialized onnx model
template <typename hit_type, typename output_type> 
class onnx_trigger : public abstract_window_trigger<hit_type, default_window_feature_vector<hit_type>>
{

    bool verbose = true;
    uint32_t vector_count_ = 1;

    class std_log_scaler
    {

        uint64_t window_count_ = 0;
        std::vector<double> means_;
        std::vector<double> stds_;

    public:
        std_log_scaler(const std::string &window_file)
        {
            std::vector<double> sum_x_;
            std::vector<double> sum_x2_;
            std::ifstream window_stream(window_file);
            if (!window_stream.is_open())
                throw std::invalid_argument("Error, could not open window file: '" + window_file + "'");
            std::string line;

            while (std::getline(window_stream, line))
            {

                if (line.size() > 0 && line[0] != '#' && line != "\n")
                {
                    std::stringstream line_stream(line);
                    default_window_feature_vector<hit_type> feature_vect;
                    line_stream >> feature_vect;
                    auto feature_vect_values = feature_vect.to_vector();
                    if (sum_x_.size() == 0)
                    {
                        sum_x_.resize(feature_vect_values.size());
                        sum_x2_.resize(feature_vect_values.size());
                    }

                    for (uint64_t i = 0; i < feature_vect_values.size(); ++i)
                    {
                        if (!std::isnan(feature_vect_values[i]))
                        {
                            sum_x_[i] += feature_vect_values[i];
                            sum_x2_[i] += feature_vect_values[i] * feature_vect_values[i];
                        }
                    }
                    ++window_count_;
                }
            }
            means_.resize(sum_x_.size());
            stds_.resize(sum_x_.size());
            for (uint64_t i = 0; i < sum_x_.size(); ++i)
            {
                means_[i] = sum_x_[i] / window_count_;
                stds_[i] = std::sqrt((sum_x2_[i] / window_count_) - means_[i] * means_[i]);
            }
        }
        std::vector<double> scale(const std::vector<double> &feat_vector)
        {
            std::vector<double> scaled_vector;
            const double epsilon_std = 0.0000001;
            scaled_vector.resize(feat_vector.size());
            for (uint32_t i = 0; i < scaled_vector.size(); ++i)
            {
                scaled_vector[i] = (feat_vector[i] - means_[i]) / (stds_[i] < epsilon_std ? 1 : stds_[i]);
            }
            return scaled_vector;
        }
    };

public:
    // pretty prints a shape dimension vector
    std::string print_shape(const std::vector<std::int64_t> &v)
    {
        std::stringstream ss("");
        for (std::size_t i = 0; i < v.size() - 1; i++)
            ss << v[i] << "x";
        ss << v[v.size() - 1];
        return ss.str();
    }

    int calculate_product(const std::vector<std::int64_t> &v)
    {
        int total = 1;
        for (auto &i : v)
            total *= i;
        return total;
    }
    //converts values from std to onnx
    template <typename T>
    Ort::Value vec_to_tensor(std::vector<T> &data, const std::vector<int64_t> &shape)
    {
        Ort::MemoryInfo mem_info =
            Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
        auto tensor = Ort::Value::CreateTensor<T>(mem_info, data.data(), data.size(), shape.data(), shape.size());
        return tensor;
    }

    using state_type = default_window_feature_vector<hit_type>;
    //std_log_scaler scaler_;
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::function<bool(output_type)> trigger_func_;
    double inference_time;

    onnx_trigger(const std::map<std::string, std::string> &args, const std::function<bool(output_type)> &trigger_func) : abstract_window_trigger<hit_type, default_window_feature_vector<hit_type>>(std::stod(args.at("trigger_time"))),
                                                                                                                         trigger_func_(trigger_func)
    {
        //onnxruntime setup

        env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_VERBOSE, "example-model-explorer");

        auto model_name = args.at("trigger_file");
        session = std::move(std::make_unique<Ort::Session>(env, get_trigger_filename<ORTCHAR_T>(model_name), session_options));

        // print name/shape of inputs
        Ort::AllocatorWithDefaultOptions allocator;
        std::vector<int64_t> input_shapes;
        std::cout << "Input Node Name/Shape (" << input_names.size() << "):" << std::endl;
        for (std::size_t i = 0; i < session->GetInputCount(); i++)
        {
            input_names.emplace_back(session->GetInputNameAllocated(i, allocator).get());
            input_shapes = session->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
            std::cout << "\t" << input_names.at(i) << " : " << print_shape(input_shapes) << std::endl;
        }
        // some models might have negative shape values to indicate dynamic shape, e.g., for variable batch size.
        for (auto &s : input_shapes)
        {
            if (s < 0)
            {
                s = 1;
            }
        }

        // print name/shape of outputs

        std::cout << "Output Node Name/Shape (" << output_names.size() << "):" << std::endl;
        for (std::size_t i = 0; i < session->GetOutputCount(); i++)
        {
            output_names.emplace_back(session->GetOutputNameAllocated(i, allocator).get());
            // auto output_shapes = session->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
            // std::cout << "\t" << output_names.at(i) << " : " << print_shape(output_shapes) << std::endl;
        }

        // assert(input_names.size() == 1 && output_names.size() == 1);
        input_shape = input_shapes;

        total_number_elements = calculate_product(input_shape);
        input_names_char = std::vector<const char *>(input_names.size(), nullptr);
        output_names_char = std::vector<const char *>(output_names.size(), nullptr);
        std::transform(std::begin(input_names), std::end(input_names), std::begin(input_names_char),
                       [&](const std::string &str)
                       { return str.c_str(); });

        std::transform(std::begin(output_names), std::end(output_names), std::begin(output_names_char),
                       [&](const std::string &str)
                       { return str.c_str(); });
    }

    std::vector<const char *> input_names_char;
    std::vector<const char *> output_names_char;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<int64_t> input_shape;
    int total_number_elements;

    std::unique_ptr<Ort::Session> session;
    const double TRIGGER_PROB_THRESHOLD = 0.5;
    //trigger decision function, cruns the model inference
    virtual bool should_trigger(const default_window_feature_vector<hit_type> &feat_vector) override
    {

        std::vector<double> input_tensor_doubles = feat_vector.to_vector(true); // scaler_.scale(feat_vector.to_vector());
        input_tensor_doubles.erase(input_tensor_doubles.begin());
        std::vector<float> input_tensor_values;
        for (double value : input_tensor_doubles)
        {
            input_tensor_values.push_back(static_cast<float>(value));
            // input_tensor_values.push_back(-1.);
        }


        std::vector<Ort::Value> input_tensors;
        input_tensors.emplace_back(vec_to_tensor<float>(input_tensor_values, input_shape));

        // double-check the dimensions of the input tensor
        assert(input_tensors[0].IsTensor() && input_tensors[0].GetTensorTypeAndShapeInfo().GetShape() == input_shape);
        try
        {
            auto output = session->Run(Ort::RunOptions{nullptr}, input_names_char.data(), input_tensors.data(),
                                       input_names_char.size(), output_names_char.data(), output_names_char.size());
            assert(output[0].IsTensor());
            auto trigger_type = output[0].GetTypeInfo();

            output_type trigger_result = (output[0].GetTensorMutableData<output_type>())[0];
            return trigger_func_(trigger_result);
        }
        catch (const Ort::Exception &exception)
        {
            std::cout << "ERROR running model inference: " << exception.what() << std::endl;
            exit(-1);
        }
    }
};
