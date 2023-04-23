#ifndef _MY_EXCEPTION_HPP_
#define _MY_EXCEPTION_HPP_

#include <string>
#include <stdexcept>

class MyException: public std::runtime_error {
public:
    MyException(std::string msg): std::runtime_error(msg) {}
    MyException(const char * msg): std::runtime_error(msg) {}
    ~MyException() noexcept {};
};



#endif