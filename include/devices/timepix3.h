#include <iomanip>
class timepix3
{
    public:
    static constexpr uint16_t size_x()
    {return 256;}
    static constexpr uint16_t size_y()
    {return 256;}
    static coord size()
    {return coord(size_x(), size_y());}
};