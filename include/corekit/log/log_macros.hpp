#pragma once

#include <sstream>
#include <string>

#include "corekit/log/ilog_manager.hpp"

namespace corekit {
namespace log {
namespace detail {

inline void CorekitLogWrite(ILogManager* logger, LogSeverity severity,
                            const std::string& message, const char* file,
                            int line) {
  if (logger == NULL) return;
  logger->LogWithSource(severity, message, file, line);
}

}  // namespace detail
}  // namespace log
}  // namespace corekit

#define COREKIT_LOG(logger, severity, message)                                \
  do {                                                                         \
    ::corekit::log::detail::CorekitLogWrite((logger), (severity), (message),  \
                                            __FILE__, __LINE__);               \
  } while (0)

#define COREKIT_LOG_STREAM(logger, severity, stream_expr)                    \
  do {                                                                        \
    std::ostringstream corekit_log_stream__;                                  \
    corekit_log_stream__ << stream_expr;                                      \
    ::corekit::log::detail::CorekitLogWrite(                                  \
        (logger), (severity), corekit_log_stream__.str(), __FILE__, __LINE__); \
  } while (0)

#define COREKIT_LOG_INFO(logger, message) \
  COREKIT_LOG((logger), ::corekit::log::LogSeverity::kInfo, (message))
#define COREKIT_LOG_WARNING(logger, message) \
  COREKIT_LOG((logger), ::corekit::log::LogSeverity::kWarning, (message))
#define COREKIT_LOG_ERROR(logger, message) \
  COREKIT_LOG((logger), ::corekit::log::LogSeverity::kError, (message))
#define COREKIT_LOG_FATAL(logger, message) \
  COREKIT_LOG((logger), ::corekit::log::LogSeverity::kFatal, (message))

#define COREKIT_LOG_INFO_S(logger, stream_expr) \
  COREKIT_LOG_STREAM((logger), ::corekit::log::LogSeverity::kInfo, stream_expr)
#define COREKIT_LOG_WARNING_S(logger, stream_expr) \
  COREKIT_LOG_STREAM((logger), ::corekit::log::LogSeverity::kWarning, stream_expr)
#define COREKIT_LOG_ERROR_S(logger, stream_expr) \
  COREKIT_LOG_STREAM((logger), ::corekit::log::LogSeverity::kError, stream_expr)
#define COREKIT_LOG_FATAL_S(logger, stream_expr) \
  COREKIT_LOG_STREAM((logger), ::corekit::log::LogSeverity::kFatal, stream_expr)
