#include "Log.hpp"

#include <iostream>
#include <crystal/vlog.h>

namespace trinity {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/

/***********************************************/
/***** static function implement ***************/
/***********************************************/
void Log::F(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_FATAL) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_FATAL, tag, format, ap);
  va_end(ap);
}

void Log::E(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_ERROR) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_ERROR, tag, format, ap);
  va_end(ap);
}

void Log::W(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_WARN) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_WARN, tag, format, ap);
  va_end(ap);
}

void Log::I(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_INFO) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_INFO, tag, format, ap);
  va_end(ap);
}

void Log::D(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_DEBUG) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_DEBUG, tag, format, ap);
  va_end(ap);
}

void Log::T(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_TRACE) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_TRACE, tag, format, ap);
  va_end(ap);
}

void Log::V(const char* tag, const char* format, ...)
{
  if (log_level < VLOG_VERBOSE) {
    return;
  }

  va_list ap;
  va_start(ap, format);
  Print(VLOG_VERBOSE, tag, format, ap);
  va_end(ap);
}

std::string Log::GetFormatMethod(const std::string& prettyFunction) {
  size_t colons = prettyFunction.find("::");
  size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
  size_t end = prettyFunction.rfind("(") - begin;
  std::string method = prettyFunction.substr(begin,end) + "()";
  return method;
}

/***********************************************/
/***** class public function implement  ********/
/***********************************************/


/***********************************************/
/***** class protected function implement  *****/
/***********************************************/


/***********************************************/
/***** class private function implement  *******/
/***********************************************/
void Log::Print(int level, const char* tag, const char* format, va_list ap)
{
  std::ignore = tag;

#ifndef NDEBUG
  auto prefix = ConvColor(level);
  std::cerr << prefix;
#endif

  vlogv(level, format, ap);

#ifndef NDEBUG
  auto suffix = ConvColor(-1);
  std::cerr << suffix;
#endif
}

inline const char* Log::ConvColor(int level)
{
  const char* ret = "\033[00m";

  switch (level) {
    case VLOG_FATAL:
      ret = "\033[1;31m";
          break;
    case VLOG_ERROR:
      ret = "\033[1;31m";
          break;
    case VLOG_WARN:
      ret = "\033[1;33m";
          break;
    case VLOG_INFO:
      ret = "\033[0;32m";
          break;
    case VLOG_DEBUG:
      ret = "\033[0;36m";
          break;
    case VLOG_TRACE:
      ret = "\033[1;34m";
          break;
    case VLOG_VERBOSE:
      ret = "\033[00m";
          break;
    default:
      break;
  }

  return ret;
}

} // namespace trinity