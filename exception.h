#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>
#include <exception>
using namespace std;

namespace drumlin {

/**
 * @brief The Exception class : inherited Exception
 */
class Exception : public std::exception
{
public:
    string message;
    Exception();
    Exception(const Exception &rhs);
    Exception(const string &str);
    const char*what(){return message.c_str();}
    void raise() const { throw *this; }
    Exception *clone() const { return new Exception(*this); }
};

} // namespace drumlin

#endif // EXCEPTION_H
