#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>
#include <sstream>
#pragma once
class not_implemented : public std::logic_error
{
    public:
    not_implemented() : std::logic_error("not yet implemented"){}
};
class basic_path
{
    protected:
    std::filesystem::path path_;
    std::filesystem::path current_path_ = std::filesystem::current_path();
    public:
    basic_path(){}
    basic_path(const std::string& path) :
    path_(path)
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
        const uint32_t sep_position = path_.string().find_last_of("/\\");
        std::string temp =  path_.string().substr(sep_position + 1);

        return temp;
    }
};
class file_path : public basic_path
{

    public:
    file_path()
    {}
    file_path(const std::string& path) :
    basic_path(path){}
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
    static void skip_eol(std::istream* stream)
    {
        if (stream->peek() < 0)
            return;
        char next_char = stream->peek();
        while (is_separator(next_char))
        {
            //FIXME only works with char = 1byte encoding (UTF8)
            next_char = stream->get();
            next_char = stream->peek();
        }
    }
    static void skip_comment_lines(std::istream* stream)
    {
        //std::cout << "skipping comments" << std::endl;
        skip_eol(stream);
        while(stream->peek() == '#')
        {
            std::string line;
            std::getline((*stream), line);
            /*std::cout << "line:" << std::endl;
            std::cout << line << std::endl;
            std::cout << "peek:" << std::endl;
            std::cout << (stream->peek()) << std::endl;*/
            skip_eol(stream);
        }    
    }
    static void skip_bom(std::istream * stream)
    {
        char* bom = new char[3];
        if (stream->peek() == 0xEF)
            (*stream).read(bom, 3);
        delete bom;
    }

};
struct coord
    {
        static constexpr uint16_t MAX_VALUE = 256;
        static constexpr uint16_t MIN_VALUE = 0;
        
        short x_; 
        short y_;
        coord(){}
        coord(short x, short y):
        x_(x),
        y_(y){}
        short x() const
        {
            return x_;
        }
        short y() const
        {
            return y_;
        }
        int32_t linearize() const
        {
            return MAX_VALUE * x_ + y_;
        }
        bool is_valid_neighbor(const coord& neighbor) const
        {
            return x_ + neighbor.x_ >= MIN_VALUE && x_ + neighbor.x_ < MAX_VALUE && y_ + neighbor.y_ >= MIN_VALUE && y_ + neighbor.y_ < MAX_VALUE;
        }
        
    };
        coord operator + (const coord & left, const coord & right)
        {
            return coord(left.x() + right.x(), left.y() + right.y());
        }