#include "corekit/json/i_json.hpp"

#include <fstream>
#include <sstream>

namespace corekit {
namespace json {

api::Result<Json> JsonCodec::Parse(const std::string& text) {
  try {
    return api::Result<Json>(Json::parse(text));
  } catch (const std::exception& ex) {
    return api::Result<Json>(
        api::Status(api::StatusCode::kInvalidArgument, std::string("json parse failed: ") + ex.what()));
  }
}

api::Result<Json> JsonCodec::LoadFile(const std::string& path) {
  std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    return api::Result<Json>(api::Status(api::StatusCode::kNotFound, "json file not found"));
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  return Parse(buffer.str());
}

api::Status JsonCodec::SaveFile(const std::string& path, const Json& value, int indent) {
  std::ofstream out(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return api::Status(api::StatusCode::kIoError, "open json file for write failed");
  }
  try {
    out << value.dump(indent);
    out << "\n";
  } catch (const std::exception& ex) {
    return api::Status(api::StatusCode::kIoError, std::string("json write failed: ") + ex.what());
  }
  return api::Status::Ok();
}

std::string JsonCodec::Dump(const Json& value, int indent) {
  try {
    return value.dump(indent);
  } catch (...) {
    return std::string();
  }
}

}  // namespace json
}  // namespace corekit
