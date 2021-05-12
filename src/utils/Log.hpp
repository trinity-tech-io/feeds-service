#ifndef _FEEDS_LOG_HPP_
#define _FEEDS_LOG_HPP_

#include <cstdarg>
#include <mutex>
#include <chrono>

namespace trinity {

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__        __FUNCSIG__
#endif

class Log {
public:
  /*** type define ***/
#define FORMAT_METHOD trinity::Log::GetFormatMethod(__PRETTY_FUNCTION__).c_str()

  /*** static function and variable ***/
  static void F(const char* tag, const char* format, ...);
  static void E(const char* tag, const char* format, ...);
  static void W(const char* tag, const char* format, ...);
  static void I(const char* tag, const char* format, ...);
  static void D(const char* tag, const char* format, ...);
  static void T(const char* tag, const char* format, ...);
  static void V(const char* tag, const char* format, ...);
  static std::string GetFormatMethod(const std::string& prettyFunction);

  struct Tag {
    static constexpr const char *Err = "Feedsd.Err";
    static constexpr const char *Util = "Feedsd.Util";

    static constexpr const char *Msg = "Feedsd.Msg";
    static constexpr const char *Rpc = "Feedsd.Rpc";
    static constexpr const char *Cmd = "Feedsd.Cmd";
    static constexpr const char *Db = "Feedsd.Db";
  };
  /*** class function and variable ***/

private:
  /*** type define ***/

  /*** static function and variable ***/
  static void Print(int level, const char* tag, const char* format, va_list ap);
  static const char* ConvColor(int level);

  /*** class function and variable ***/
  explicit Log() = delete;
  virtual ~Log() = delete;
};

} // namespace trinity

#endif /* _FEEDS_LOG_HPP_ */
