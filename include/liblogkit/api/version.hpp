#pragma once

#include <cstdint>

namespace liblogkit {
namespace api {

static const std::uint32_t kApiVersionMajor = 1;
static const std::uint32_t kApiVersionMinor = 0;
static const std::uint32_t kApiVersionPatch = 0;
static const std::uint32_t kApiVersion =
    (kApiVersionMajor << 16) | (kApiVersionMinor << 8) | kApiVersionPatch;

}  // namespace api
}  // namespace liblogkit
