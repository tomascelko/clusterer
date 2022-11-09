#pragma once
#include <functional>
template <typename data_type>
class multi_pipe_writer
{
    using writer_type = pipe_writer<data_type>;
    using split_descriptor_type = pipe_descriptor<data_type>;
    
    std::vector<writer_type> writers_;
    split_descriptor_type* split_descriptor_;
    public:
    multi_pipe_writer(split_descriptor_type * split_descriptor) :
    split_descriptor_(split_descriptor)
    {
        
    }
    
    void add_pipe(default_pipe<data_type>* pipe)
    {
        writers_.emplace_back(writer_type{pipe});
    }

    bool write(data_type && data)
    {
        writers_[split_descriptor_->get_pipe_index(data)].write(std::move(data));
        return true;
    }
    void flush()
    {
        for(auto & writer : writers_)
        {
            writer.flush();
        }
    }
    void close()
    {
        for(auto & writer : writers_)
        {
            writer.write(data_type::end_token());
            writer.flush();
        }
    }
    ~multi_pipe_writer()
    {
        if(dynamic_cast<trivial_clustering_descriptor<data_type>*>(split_descriptor_) != nullptr)
        {
            delete split_descriptor_; //in this case, we are the owners of the trivial descriptor
        }
        writers_.clear();
    }
};