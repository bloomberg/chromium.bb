// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "connections/implementation/mediums/utils.h"

#include <memory>
#include <string>

#include "internal/platform/prng.h"
#include "internal/platform/crypto.h"

namespace location {
namespace nearby {
namespace connections {

namespace {
constexpr absl::string_view kUpgradeServiceIdPostfix = "_UPGRADE";
}

ByteArray Utils::GenerateRandomBytes(size_t length) {
  Prng rng;
  std::string data;
  data.reserve(length);

  // Adds 4 random bytes per iteration.
  while (length > 0) {
    std::uint32_t val = rng.NextUint32();
    for (int i = 0; i < 4; i++) {
      data += val & 0xFF;
      val >>= 8;
      length--;

      if (!length) break;
    }
  }

  return ByteArray(data);
}

ByteArray Utils::Sha256Hash(const ByteArray& source, size_t length) {
  return Utils::Sha256Hash(std::string(source), length);
}

ByteArray Utils::Sha256Hash(const std::string& source, size_t length) {
  ByteArray full_hash(length);
  full_hash.CopyAt(0, Crypto::Sha256(source));
  return full_hash;
}

std::string Utils::WrapUpgradeServiceId(const std::string& service_id) {
  if (service_id.empty()) {
    return {};
  }
  return service_id + std::string(kUpgradeServiceIdPostfix);
}

std::string Utils::UnwrapUpgradeServiceId(
    const std::string& upgrade_service_id) {
  auto pos = upgrade_service_id.find(std::string(kUpgradeServiceIdPostfix));
  if (pos != std::string::npos) {
    return std::string(upgrade_service_id, 0, pos);
  }
  return upgrade_service_id;
}

LocationHint Utils::BuildLocationHint(const std::string& location) {
  LocationHint location_hint;
  location_hint.set_format(LocationStandard::UNKNOWN);

  if (!location.empty()) {
    location_hint.set_location(location);
    if (location.at(0) == '+') {
      location_hint.set_format(LocationStandard::E164_CALLING);
    } else {
      location_hint.set_format(LocationStandard::ISO_3166_1_ALPHA_2);
    }
  }
  return location_hint;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
