#include <Common/ExceptionFormatterDefault.h>

#include <sstream>

namespace DB
{

std::string ExceptionFormatterDefault::format() const
{
    std::stringstream stream;
    
    if (class_name)
        stream << *class_name << ". ";
    stream << "Code: " << code;
    if (type)
        stream << ", " << *message;
    if (version)
        stream << " (version " << *version << ")";

    return stream.str();
}

}
