#ifndef _ELASOTS_LOG_HPP_
#define _ELASOTS_LOG_HPP_

#include <cstdarg>
#include <mutex>
#include <chrono>

class Log {
public:
  /*** type define ***/
#define FORMAT_METHOD Log::GetFormatMethod(__PRETTY_FUNCTION__).c_str()

  enum Level {
    Warn = 0,
    Info,
    Debug,
    Verbose,
    Trace,
  };

  /*** static function and variable ***/
  static void SetLevel(enum Level level);
  static void F(const char* tag, const char* format, ...);
  static void E(const char* tag, const char* format, ...);
  static void W(const char* tag, const char* format, ...);
  static void I(const char* tag, const char* format, ...);
  static void D(const char* tag, const char* format, ...);
  static void V(const char* tag, const char* format, ...);
  static void T(const char* tag, const char* function, int line, const char* format, ...);
  static uint64_t Now();
  static uint64_t MilliNow();
  static uint64_t MicroNow();
  static uint64_t NanoNow();
  static std::string GetFormatMethod(const std::string& prettyFunction);

  static constexpr const char *TAG = "elastos";

  /*** class function and variable ***/

private:
  /*** type define ***/

  /*** static function and variable ***/
  static inline void log(const char head, const char* tag, const char* format, ...);
  static inline void log(const char head, const char* tag, const char* format, va_list ap);

  static enum Level m_logLevel;
  static std::recursive_mutex m_mutex;
//  static const std::chrono::steady_clock::time_point m_baseTime;

  static inline const char* convColor(const char head);
#if defined(__ANDROID__)
  static inline int convPrio(const char head);
#endif

  /*** class function and variable ***/
  explicit Log() = delete;
  virtual ~Log() = delete;
};

#endif /* _ELASOTS_LOG_HPP_ */
