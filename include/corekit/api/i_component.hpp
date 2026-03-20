#pragma once

#include <cstdint>

namespace corekit {
namespace api {

/// Base interface for all corekit components.
/// Provides uniform lifecycle, introspection, and version checking.
class IComponent {
 public:
  virtual ~IComponent() {}

  /// Human-readable implementation name for logging and diagnostics.
  virtual const char* Name() const = 0;

  /// API version this instance was compiled against.
  /// Use at runtime to verify DLL/header compatibility.
  virtual std::uint32_t ApiVersion() const = 0;

  /// Release the instance. Pointer is invalid after this call.
  virtual void Release() = 0;
};

}  // namespace api
}  // namespace corekit
