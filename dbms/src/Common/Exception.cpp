#include "Exception.h"

#include <string.h>
#include <cxxabi.h>
#include <optional>
#include <Poco/String.h>
#include <common/logger_useful.h>
#include <IO/WriteHelpers.h>
#include <IO/Operators.h>
#include <IO/ReadBufferFromString.h>
#include <common/demangle.h>
#include <Common/config_version.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int POCO_EXCEPTION;
    extern const int STD_EXCEPTION;
    extern const int UNKNOWN_EXCEPTION;
    extern const int CANNOT_TRUNCATE_FILE;
    extern const int NOT_IMPLEMENTED;
}

std::string errnoToString(int code, int e)
{
    const size_t buf_size = 128;
    char buf[buf_size];
#ifndef _GNU_SOURCE
    int rc = strerror_r(e, buf, buf_size);
#ifdef __APPLE__
    if (rc != 0 && rc != EINVAL)
#else
    if (rc != 0)
#endif
    {
        std::string tmp = std::to_string(code);
        const char * code_str = tmp.c_str();
        const char * unknown_message = "Unknown error ";
        strcpy(buf, unknown_message);
        strcpy(buf + strlen(unknown_message), code_str);
    }
    return "errno: " + toString(e) + ", strerror: " + std::string(buf);
#else
    (void)code;
    return "errno: " + toString(e) + ", strerror: " + std::string(strerror_r(e, buf, sizeof(buf)));
#endif
}

void throwFromErrno(const std::string & s, int code, int e)
{
    throw ErrnoException(s + ", " + errnoToString(code, e), code, e);
}

void tryLogCurrentException(const char * log_name, const std::string & start_of_message)
{
    tryLogCurrentException(&Logger::get(log_name), start_of_message);
}

void tryLogCurrentException(Poco::Logger * logger, const std::string & start_of_message)
{
    try
    {
        LOG_ERROR(logger, start_of_message << (start_of_message.empty() ? "" : ": ") << getCurrentExceptionMessage(true));
    }
    catch (...)
    {
    }
}

std::string getCurrentExceptionMessage(bool with_stacktrace, bool check_embedded_stacktrace, ExceptionFormatter &&formatter)
{
    std::stringstream stream;

    try
    {
        throw;
    }
    catch (const Exception & e)
    {
        *formatter.version = VERSION_STRING VERSION_OFFICIAL;
        return getExceptionMessage(e, with_stacktrace, check_embedded_stacktrace, std::move(formatter));
    }
    catch (const Poco::Exception & e)
    {
        try
        {
            formatter.code = ErrorCodes::POCO_EXCEPTION;
            *formatter.class_name = "Poco::Exception";
            *formatter.version = VERSION_STRING VERSION_OFFICIAL;
            *formatter.message = "e.code() = " + toString(e.code()) + ", e.displayText() = " + e.displayText();
        }
        catch (...) {}
    }
    catch (const std::exception & e)
    {
        try
        {
            formatter.code = ErrorCodes::STD_EXCEPTION;
            *formatter.class_name = "std::exception";
            *formatter.version = VERSION_STRING VERSION_OFFICIAL;
            int status;
            *formatter.type = demangle(typeid(e).name(), status);
            if (status)
                *formatter.demangling_status = status;
            *formatter.message = "e.what() = " + std::string(e.what());
        }
        catch (...) {}
    }
    catch (...)
    {
        try
        {
            formatter.code = ErrorCodes::UNKNOWN_EXCEPTION;
            *formatter.class_name = "Unknown exception";
            int status = 0;
            *formatter.type = demangle(abi::__cxa_current_exception_type()->name(), status);
            if (status)
                *formatter.demangling_status = status;
            *formatter.version = VERSION_STRING VERSION_OFFICIAL;
        }
        catch (...) {}
    }

    return formatter.format();
}



int getCurrentExceptionCode()
{
    try
    {
        throw;
    }
    catch (const Exception & e)
    {
        return e.code();
    }
    catch (const Poco::Exception &)
    {
        return ErrorCodes::POCO_EXCEPTION;
    }
    catch (const std::exception &)
    {
        return ErrorCodes::STD_EXCEPTION;
    }
    catch (...)
    {
        return ErrorCodes::UNKNOWN_EXCEPTION;
    }
}


void rethrowFirstException(const Exceptions & exceptions)
{
    for (size_t i = 0, size = exceptions.size(); i < size; ++i)
        if (exceptions[i])
            std::rethrow_exception(exceptions[i]);
}


void tryLogException(std::exception_ptr e, const char * log_name, const std::string & start_of_message)
{
    try
    {
        std::rethrow_exception(std::move(e));
    }
    catch (...)
    {
        tryLogCurrentException(log_name, start_of_message);
    }
}

void tryLogException(std::exception_ptr e, Poco::Logger * logger, const std::string & start_of_message)
{
    try
    {
        std::rethrow_exception(std::move(e));
    }
    catch (...)
    {
        tryLogCurrentException(logger, start_of_message);
    }
}

std::string getExceptionMessage(const Exception & e, bool with_stacktrace, bool check_embedded_stacktrace, ExceptionFormatter &&formatter)
{
    try
    {
        std::string text = e.displayText();

        bool has_embedded_stack_trace = false;
        if (check_embedded_stacktrace)
        {
            auto embedded_stack_trace_pos = text.find("Stack trace");
            has_embedded_stack_trace = embedded_stack_trace_pos != std::string::npos;
            if (!with_stacktrace && has_embedded_stack_trace)
            {
                text.resize(embedded_stack_trace_pos);
                Poco::trimRightInPlace(text);
            }
        }

        formatter.code = e.code();
        *formatter.type = e.name();
        *formatter.message = e.message();

        if (with_stacktrace && !has_embedded_stack_trace)
            *formatter.stack_trace = e.getStackTrace().toString();
    }
    catch (...) {}

    return formatter.format();
}

std::string getExceptionMessage(std::exception_ptr e, bool with_stacktrace, ExceptionFormatter &&formatter)
{
    try
    {
        std::rethrow_exception(std::move(e));
    }
    catch (...)
    {
        return getCurrentExceptionMessage(with_stacktrace, false, std::move(formatter));
    }
}


std::string ExecutionStatus::serializeText() const
{
    WriteBufferFromOwnString wb;
    wb << code << "\n" << escape << message;
    return wb.str();
}

void ExecutionStatus::deserializeText(const std::string & data)
{
    ReadBufferFromString rb(data);
    rb >> code >> "\n" >> escape >> message;
}

bool ExecutionStatus::tryDeserializeText(const std::string & data)
{
    try
    {
        deserializeText(data);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

ExecutionStatus ExecutionStatus::fromCurrentException(const std::string & start_of_message)
{
    String msg = (start_of_message.empty() ? "" : (start_of_message + ": ")) + getCurrentExceptionMessage(false, true);
    return ExecutionStatus(getCurrentExceptionCode(), msg);
}


}
