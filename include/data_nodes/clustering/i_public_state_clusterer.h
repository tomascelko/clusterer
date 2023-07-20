#include <list>
#include <vector>
template <typename unfinished_cl_type>
class i_public_state_clusterer
{
public:
    using unfinished_cl_iterator = typename std::list<unfinished_cl_type>::iterator;

    using unfinished_cl_its = std::vector<unfinished_cl_iterator>;
    virtual bool open_cl_exists() = 0;
    virtual unfinished_cl_its get_all_unfinished_clusters() = 0;

    ~i_public_state_clusterer() = default;
};