#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "corekit/api/export.hpp"

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

enum class ErrorModule : std::uint8_t {
  kCore = 0x00,
  kApi = 0x01,
  kLog = 0x10,
  kIpc = 0x20,
  kMemory = 0x30,
  kConcurrent = 0x40,
  kTask = 0x50,
  kJson = 0x60,
};

struct ErrorCatalogEntry {
  std::uint32_t hex_code;
  const char* symbol;
  const char* description;
};

// Code layout: 0xMMSDDDDD
// - MM: module id
// - S: status code family (4 bits)
// - DDDDD: module-local detail id (20 bits)
COREKIT_API std::uint32_t MakeErrorCode(ErrorModule module, StatusCode status_code,
                                        std::uint32_t detail_id = 0);
COREKIT_API const char* ErrorModuleName(ErrorModule module);
COREKIT_API const char* StatusCodeName(StatusCode status_code);
COREKIT_API const ErrorCatalogEntry* FindErrorCatalogEntry(std::uint32_t hex_code);
COREKIT_API std::string FormatErrorCodeHex(std::uint32_t hex_code);

class Status {
 public:
  Status() : code_(StatusCode::kOk), hex_code_(MakeErrorCode(ErrorModule::kCore, code_)) {}
  Status(StatusCode code, std::string message)
      : code_(code), message_(std::move(message)),
        hex_code_(MakeErrorCode(ErrorModule::kCore, code_)) {}
  Status(StatusCode code, std::string message, ErrorModule module, std::uint32_t detail_id = 0)
      : code_(code),
        message_(std::move(message)),
        hex_code_(MakeErrorCode(module, code_, detail_id)) {}
  Status(StatusCode code, std::string message, std::uint32_t hex_code)
      : code_(code), message_(std::move(message)), hex_code_(hex_code) {}

  static Status Ok() { return Status(); }
  static Status FromModule(StatusCode code, std::string message, ErrorModule module,
                           std::uint32_t detail_id = 0) {
    return Status(code, std::move(message), module, detail_id);
  }

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }
  const std::string& message() const { return message_; }
  std::uint32_t hex_code() const { return hex_code_; }
  std::string hex_code_string() const { return FormatErrorCodeHex(hex_code_); }

 private:
  StatusCode code_;
  std::string message_;
  std::uint32_t hex_code_;
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


