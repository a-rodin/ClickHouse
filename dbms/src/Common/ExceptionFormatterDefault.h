#pragma once

#include <Common/ExceptionFormatter.h>

namespace DB
{

struct ExceptionFormatterDefault: public ExceptionFormatter
{
    std::string format() const override;
};

}
