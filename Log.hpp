#pragma once

#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/time.h>

enum Severity {INFO, WARN, ERROR};

static const std::map<Severity, const char*> SeverityString = {
  {INFO, "I"},
  {WARN, "W"},
  {ERROR, "E"}
};

#define LOG(s) (Log(s))

inline static int64_t getTime()
{
  struct timeval tv;
  ::gettimeofday(&tv, nullptr);
  return (tv.tv_sec * 1000000000L) + (tv.tv_usec * 1000L);
}

/**
 * Simple class for logging messages
 */
class Log
{
  public:

    // Print log header in constructor
    Log(Severity s)
    {
      struct timeval tv;
      gettimeofday(&tv, nullptr);

      _stream << "[" << SeverityString.at(s) << "] ";
      _stream << std::put_time(std::localtime(&(tv.tv_sec)), "<%m/%d %H:%M:%S.")
              << std::setfill('0') << std::setw(6) << tv.tv_usec << "> ";
      _stream << "| ";
    }

    // Print newline and flush in destructor
    ~Log()
    {
      std::cout << _stream.str() << std::endl;
    }

    Log(Log&&) = delete;
    Log(const Log&) = delete;
    void operator=(const Log&) = delete;
    void operator=(Log&&) = delete;

    // Log mesg, and return self for chaining
    template<typename T>
    inline Log& operator<<(T mesg)
    {
      _stream << mesg;
      return *this;
    }

    // Log operator specification for name-value pairs
    template<typename T, typename U>
    inline Log& operator<<(const std::pair<T,U>& p)
    {
      _stream << " " << p.first << "=(" << p.second << ")";
      return *this;
    }

  private:

    std::stringstream _stream;
};
