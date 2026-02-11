#pragma once

#include <string>

namespace corekit {
namespace api {

enum class StatusCode {
  kOk = 0,
  kInvalidArgument,
  kNotInitialized,
  kAlreadyInitialized,
  kNotFound,
  kWouldBlock,
  kBufferTooSmall,
  kIoError,
  kInternalError,
  kUnsupported
};

class Status {
 public:
  Status() : code_(StatusCode::kOk) {}
  Status(StatusCode code, std::string message) : code_(code), message_(message) {}

  static Status Ok() { return Status(); }

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  StatusCode code_;
  std::string message_;
};

template <typename T>
class Result {
 public:
  Result(const Status& status) : status_(status), has_value_(false), value_() {}
  Result(const T& value) : status_(Status::Ok()), has_value_(true), value_(value) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  const T& value() const { return value_; }
  T& value() { return value_; }

 private:
  Status status_;
  bool has_value_;
  T value_;
};

}  // namespace api
}  // namespace corekit

