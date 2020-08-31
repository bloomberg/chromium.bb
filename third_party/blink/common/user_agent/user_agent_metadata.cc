// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#include "base/pickle.h"
#include "net/http/structured_headers.h"

namespace blink {

namespace {
constexpr uint32_t kVersion = 2u;
}  // namespace

UserAgentBrandVersion::UserAgentBrandVersion(
    const std::string& ua_brand,
    const std::string& ua_major_version) {
  brand = ua_brand;
  major_version = ua_major_version;
}

const std::string UserAgentMetadata::SerializeBrandVersionList() {
  net::structured_headers::List brand_version_header =
      net::structured_headers::List();
  for (const UserAgentBrandVersion& brand_version : brand_version_list) {
    if (brand_version.major_version.empty()) {
      brand_version_header.push_back(
          net::structured_headers::ParameterizedMember(
              net::structured_headers::Item(brand_version.brand), {}));
    } else {
      brand_version_header.push_back(
          net::structured_headers::ParameterizedMember(
              net::structured_headers::Item(brand_version.brand),
              {std::make_pair("v", net::structured_headers::Item(
                                       brand_version.major_version))}));
    }
  }

  return net::structured_headers::SerializeList(brand_version_header)
      .value_or("");
}

// static
base::Optional<std::string> UserAgentMetadata::Marshal(
    const base::Optional<UserAgentMetadata>& in) {
  if (!in)
    return base::nullopt;
  base::Pickle out;
  out.WriteUInt32(kVersion);

  out.WriteUInt32(in->brand_version_list.size());
  for (const auto& brand_version : in->brand_version_list) {
    out.WriteString(brand_version.brand);
    out.WriteString(brand_version.major_version);
  }

  out.WriteString(in->full_version);
  out.WriteString(in->platform);
  out.WriteString(in->platform_version);
  out.WriteString(in->architecture);
  out.WriteString(in->model);
  out.WriteBool(in->mobile);
  return std::string(reinterpret_cast<const char*>(out.data()), out.size());
}

// static
base::Optional<UserAgentMetadata> UserAgentMetadata::Demarshal(
    const base::Optional<std::string>& encoded) {
  if (!encoded)
    return base::nullopt;

  base::Pickle pickle(encoded->data(), encoded->size());
  base::PickleIterator in(pickle);

  uint32_t version;
  UserAgentMetadata out;
  if (!in.ReadUInt32(&version) || version != kVersion)
    return base::nullopt;

  uint32_t brand_version_size;
  if (!in.ReadUInt32(&brand_version_size))
    return base::nullopt;
  for (uint32_t i = 0; i < brand_version_size; i++) {
    UserAgentBrandVersion brand_version;
    if (!in.ReadString(&brand_version.brand))
      return base::nullopt;
    if (!in.ReadString(&brand_version.major_version))
      return base::nullopt;
    out.brand_version_list.push_back(std::move(brand_version));
  }

  if (!in.ReadString(&out.full_version))
    return base::nullopt;
  if (!in.ReadString(&out.platform))
    return base::nullopt;
  if (!in.ReadString(&out.platform_version))
    return base::nullopt;
  if (!in.ReadString(&out.architecture))
    return base::nullopt;
  if (!in.ReadString(&out.model))
    return base::nullopt;
  if (!in.ReadBool(&out.mobile))
    return base::nullopt;
  return base::make_optional(std::move(out));
}

bool UserAgentBrandVersion::operator==(const UserAgentBrandVersion& a) const {
  return a.brand == brand && a.major_version == major_version;
}

bool operator==(const UserAgentMetadata& a, const UserAgentMetadata& b) {
  return a.brand_version_list == b.brand_version_list &&
         a.full_version == b.full_version && a.platform == b.platform &&
         a.platform_version == b.platform_version &&
         a.architecture == b.architecture && a.model == b.model &&
         a.mobile == b.mobile;
}

bool operator==(const UserAgentOverride& a, const UserAgentOverride& b) {
  return a.ua_string_override == b.ua_string_override &&
         a.ua_metadata_override == b.ua_metadata_override;
}

}  // namespace blink
