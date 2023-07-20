#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>
#include <sstream>
#include <cmath>
#include <algorithm>
#pragma once
class not_implemented : public std::logic_error
{
public:
    not_implemented() : std::logic_error("not yet implemented") {}
};
class basic_path
{
protected:
    std::filesystem::path path_;
    std::filesystem::path current_path_ = std::filesystem::current_path();

public:
    basic_path() {}
    basic_path(const std::string &path) : path_(path)
    {
    }

    std::string as_absolute() const
    {
        return std::filesystem::absolute(path_).string();
    }
    std::string as_relative() const
    {
        return std::filesystem::relative(current_path_, path_).string();
    }
    std::string last_folder() const
    {
        const int32_t sep_position = path_.string().find_last_of("/\\");
        std::string temp = path_.string().substr(sep_position + 1);

        return temp;
    }
    std::string parent() const
    {
        return std::filesystem::absolute(path_.parent_path()).string();
    }
};
class file_path : public basic_path
{

public:
    file_path()
    {
    }
    file_path(const std::string &path) : basic_path(path) {}
    std::string filename() const
    {
        return path_.filename().string();
    }
};
class io_utils
{
    static constexpr int CR = 13;
    static constexpr int LF = 10;

private:
    static bool is_separator(char character)
    {
        const int SEPARATORS_ASCII_IDEX = 16;
        return character < SEPARATORS_ASCII_IDEX && character > 0;
    }

public:
    static void skip_eol(std::istream &stream)
    {
        if (stream.peek() < 0)
            return;
        char next_char = stream.peek();
        while (is_separator(next_char))
        {
            // FIXME only works with char = 1byte encoding (UTF8)
            next_char = stream.get();
            next_char = stream.peek();
        }
    }
    static void skip_comment_lines(std::istream &stream)
    {
        // std::cout << "skipping comments" << std::endl;
        skip_eol(stream);
        while (stream.peek() == '#')
        {
            std::string line;
            std::getline((stream), line);
            /*std::cout << "line:" << std::endl;
            std::cout << line << std::endl;
            std::cout << "peek:" << std::endl;
            std::cout << (stream->peek()) << std::endl;*/
            skip_eol(stream);
        }
    }
    static void skip_bom(std::istream &stream)
    {
        char *bom = new char[3];
        if (stream.peek() == 0xEF)
            stream.read(bom, 3);
        delete bom;
    }

    static std::string strip(const std::string &value)
    {
        uint32_t start_index = value.size();
        uint32_t end_index = 0;

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (!is_separator(*it))
            {
                start_index = it - value.begin();
                break;
            }
        }
        for (auto it = value.rend(); it != value.rbegin(); ++it)
        {
            if (!is_separator(*it))
            {
                end_index = it - value.rbegin();
                break;
            }
        }
        if (start_index >= end_index)
            return "";
        return value.substr(start_index, end_index + 1);
    }
};
struct coord
{
    static constexpr uint16_t MAX_VALUE = 256;
    static constexpr uint16_t MIN_VALUE = 0;

    short x_;
    short y_;
    coord() {}
    coord(short x, short y) : x_(x),
                              y_(y) {}
    short x() const
    {
        return x_;
    }
    short y() const
    {
        return y_;
    }
    int32_t linearize() const // use if untiled coord -> untiled linear
    {
        return (MAX_VALUE)*x_ + y_;
    }
    int32_t linearize(uint32_t tile_size) const // use if untiled coord -> tiled linear
    {
        return (MAX_VALUE / tile_size) * (x_ / tile_size) + (y_ / tile_size);
    }
    int32_t linearize_tiled(uint32_t tile_size) const // use if tiled coord -> tiled linear
    {
        return (MAX_VALUE / tile_size) * x_ + y_;
    }
    bool is_valid_neighbor(const coord &neighbor, int32_t tile_size = 1) const
    {
        // Note expression obtained after multiplying the whole formula by tile_size
        return (x_ + neighbor.x_ * tile_size >= MIN_VALUE && x_ + neighbor.x_ * tile_size < MAX_VALUE && y_ + neighbor.y_ * tile_size >= MIN_VALUE && y_ + neighbor.y_ * tile_size < MAX_VALUE);
    }
    coord operator+(const coord &right)
    {
        return coord(this->x() + right.x(), this->y() + right.y());
    }
};

template <typename number_type>
std::string double_to_str(number_type number, uint16_t precision = 6)
{
    std::string result;
    const uint16_t MAX_LENGTH = 20;
    int64_t number_rounded;
    std::string str;
    str.reserve(MAX_LENGTH);
    const double EPSILON = 0.0000000000000001;
    for (uint16_t i = 0; i < precision; ++i)
    {
        number *= 10;
    }
    number_rounded = (int64_t)std::round(number);
    for (uint16_t i = 0; i < MAX_LENGTH; ++i)
    {
        str.push_back((char)(48 + (number_rounded % 10)));
        if (i == precision - 1)
            str.push_back('.');
        number_rounded /= 10;
        // number = std::floor(number);
        if (number_rounded == 0)
        {
            if (i == precision - 1)
                str.push_back('0');
            break;
        }
    }
    std::reverse(str.begin(), str.end());
    return str;
}
template <typename T, class enable = void>
class class_exists
{
    static constexpr bool value = false;
};
template <typename T>
class class_exists<T, std::enable_if_t<(sizeof(T) > 0)>>
{
    static constexpr bool value = true;
};

static bool ends_with(const std::string &str, const std::string &suffix)
{
    return str.size() > suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}
template <typename T>
static typename std::vector<T>::iterator binary_find(std::vector<T> &array, const T &item)
{
    if (array.size() == 0)
        return array.end();
    return binary_find_impl(array, item, 0, array.size());
}
template <typename T>
static typename std::vector<T>::iterator binary_find_impl(std::vector<T> &array,
                                                          const T &item, uint32_t left, uint32_t right)
{
    uint32_t mid = (left + right) / 2;
    if (left == mid)
    {
        if (array[left] == item)
            return array.begin() + left;
        else
            return array.end();
    }
    if (array[mid] == item)
        return array.begin() + mid;
    if (array[mid] < item)
        return binary_find_impl(array, item, mid, right);
    return binary_find_impl(array, item, left, mid);
}
