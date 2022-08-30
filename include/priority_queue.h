/*#include <vector>
#include <functional>
template <typename type,
    typename comparer = std::less<typename type::value_type>>
class priority_queue : public std::set<T, comparer> {
public:
    type pop() {
    std::pop_heap(c.begin(), c.end(), comp);
    type value = std::move(c.back());
    pop_back();
    return value;
  }

};*/