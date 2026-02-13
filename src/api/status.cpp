#include "corekit/api/status.hpp"

#include <cstdio>

namespace corekit {
namespace api {

namespace {

inline std::uint32_t PackErrorCode(std::uint8_t module, std::uint8_t status, std::uint32_t detail) {
  return (static_cast<std::uint32_t>(module) << 24) |
         ((static_cast<std::uint32_t>(status) & 0x0Fu) << 20) |
         (detail & 0x000FFFFFu);
}

#define COREKIT_ECODE(module, status, detail) \
  PackErrorCode(static_cast<std::uint8_t>(module), static_cast<std::uint8_t>(status), detail)

static const ErrorCatalogEntry kErrorCatalog[] = {
    // Core generic status family (detail id = 0)
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kOk, 0x0000), "CORE_OK",
     "Operation succeeded"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kInvalidArgument, 0x0000),
     "CORE_INVALID_ARGUMENT", "Invalid argument"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kNotInitialized, 0x0000),
     "CORE_NOT_INITIALIZED", "Object not initialized"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kAlreadyInitialized, 0x0000),
     "CORE_ALREADY_INITIALIZED", "Object already initialized"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kNotFound, 0x0000), "CORE_NOT_FOUND",
     "Resource not found"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kWouldBlock, 0x0000), "CORE_WOULD_BLOCK",
     "Operation would block"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kBufferTooSmall, 0x0000),
     "CORE_BUFFER_TOO_SMALL", "Buffer is too small"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kIoError, 0x0000), "CORE_IO_ERROR",
     "I/O error"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kInternalError, 0x0000),
     "CORE_INTERNAL_ERROR", "Internal error"},
    {COREKIT_ECODE(ErrorModule::kCore, StatusCode::kUnsupported, 0x0000), "CORE_UNSUPPORTED",
     "Operation unsupported"},

    // Module examples/detail ids; keep appending here as a unified lookup table.
    {COREKIT_ECODE(ErrorModule::kTask, StatusCode::kWouldBlock, 0x0001),
     "TASK_QUEUE_FULL", "Task queue is full"},
    {COREKIT_ECODE(ErrorModule::kTask, StatusCode::kInvalidArgument, 0x0001),
     "TASK_INVALID_FN", "Task function is null"},
    {COREKIT_ECODE(ErrorModule::kIpc, StatusCode::kWouldBlock, 0x0001),
     "IPC_QUEUE_FULL", "IPC channel queue is full"},
    {COREKIT_ECODE(ErrorModule::kIpc, StatusCode::kWouldBlock, 0x0002),
     "IPC_QUEUE_EMPTY", "IPC channel has no message"},
    {COREKIT_ECODE(ErrorModule::kMemory, StatusCode::kInvalidArgument, 0x0001),
     "MEM_INVALID_ALIGNMENT", "Invalid memory alignment"},
    {COREKIT_ECODE(ErrorModule::kJson, StatusCode::kInvalidArgument, 0x0001),
     "JSON_PARSE_FAILED", "JSON parse failed"},
    {COREKIT_ECODE(ErrorModule::kConcurrent, StatusCode::kWouldBlock, 0x0001),
     "QUEUE_FULL", "Concurrent queue is full"},
    {COREKIT_ECODE(ErrorModule::kConcurrent, StatusCode::kWouldBlock, 0x0002),
     "QUEUE_EMPTY", "Concurrent queue is empty"},
};

#undef COREKIT_ECODE

}  // namespace

std::uint32_t MakeErrorCode(ErrorModule module, StatusCode status_code, std::uint32_t detail_id) {
  return PackErrorCode(static_cast<std::uint8_t>(module),
                       static_cast<std::uint8_t>(status_code), detail_id);
}

const char* ErrorModuleName(ErrorModule module) {
  switch (module) {
    case ErrorModule::kCore:
      return "core";
    case ErrorModule::kApi:
      return "api";
    case ErrorModule::kLog:
      return "log";
    case ErrorModule::kIpc:
      return "ipc";
    case ErrorModule::kMemory:
      return "memory";
    case ErrorModule::kConcurrent:
      return "concurrent";
    case ErrorModule::kTask:
      return "task";
    case ErrorModule::kJson:
      return "json";
    default:
      return "unknown";
  }
}

const char* StatusCodeName(StatusCode status_code) {
  switch (status_code) {
    case StatusCode::kOk:
      return "kOk";
    case StatusCode::kInvalidArgument:
      return "kInvalidArgument";
    case StatusCode::kNotInitialized:
      return "kNotInitialized";
    case StatusCode::kAlreadyInitialized:
      return "kAlreadyInitialized";
    case StatusCode::kNotFound:
      return "kNotFound";
    case StatusCode::kWouldBlock:
      return "kWouldBlock";
    case StatusCode::kBufferTooSmall:
      return "kBufferTooSmall";
    case StatusCode::kIoError:
      return "kIoError";
    case StatusCode::kInternalError:
      return "kInternalError";
    case StatusCode::kUnsupported:
      return "kUnsupported";
    default:
      return "kUnknown";
  }
}

const ErrorCatalogEntry* FindErrorCatalogEntry(std::uint32_t hex_code) {
  for (std::size_t i = 0; i < sizeof(kErrorCatalog) / sizeof(kErrorCatalog[0]); ++i) {
    if (kErrorCatalog[i].hex_code == hex_code) {
      return &kErrorCatalog[i];
    }
  }
  return NULL;
}

std::string FormatErrorCodeHex(std::uint32_t hex_code) {
  char buf[11] = {0};  // "0xFFFFFFFF"
  std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned int>(hex_code));
  return std::string(buf);
}

}  // namespace api
}  // namespace corekit



