#pragma once

#include <string>

#include "3party/json.hpp"
#include "corekit/api/export.hpp"
#include "corekit/api/status.hpp"

namespace corekit {
namespace json {

using Json = nlohmann::json;

class COREKIT_API JsonCodec {
 public:
  // Parse JSON text into a DOM object.
  // Returns kInvalidArgument when text is not valid JSON.
  static api::Result<Json> Parse(const std::string& text);

  // Load and parse a JSON file from disk.
  // Returns kNotFound when file does not exist.
  static api::Result<Json> LoadFile(const std::string& path);

  // Serialize JSON to file.
  // Returns kIoError on write failures.
  static api::Status SaveFile(const std::string& path, const Json& value, int indent = 2);

  // Serialize JSON to UTF-8 string for logging/debugging.
  static std::string Dump(const Json& value, int indent = 2);
};

}  // namespace json
}  // namespace corekit
