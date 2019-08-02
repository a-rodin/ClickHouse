#pragma once

#include <optional>
#include <string>

namespace DB
{

struct ExceptionFormatter
{
    int code;
    std::optional<std::string> class_name;
    std::optional<std::string> type;
    std::optional<std::string> message;
    std::optional<std::string> stack_trace;
    std::optional<int> demangling_status;
    std::optional<std::string> version;

    virtual std::string format() const = 0;
    virtual ~ExceptionFormatter() {}
};

}
