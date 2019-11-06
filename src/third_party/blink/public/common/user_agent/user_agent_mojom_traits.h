// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#include <string>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/user_agent/user_agent_metadata.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::UserAgentMetadataDataView,
                                        ::blink::UserAgentMetadata> {
  static const std::string& brand(const ::blink::UserAgentMetadata& data) {
    return data.brand;
  }
  static const std::string& full_version(
      const ::blink::UserAgentMetadata& data) {
    return data.full_version;
  }
  static const std::string& major_version(
      const ::blink::UserAgentMetadata& data) {
    return data.major_version;
  }
  static const std::string& platform(const ::blink::UserAgentMetadata& data) {
    return data.platform;
  }
  static const std::string& architecture(
      const ::blink::UserAgentMetadata& data) {
    return data.architecture;
  }
  static const std::string& model(const ::blink::UserAgentMetadata& data) {
    return data.model;
  }

  static bool Read(blink::mojom::UserAgentMetadataDataView data,
                   ::blink::UserAgentMetadata* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_MOJOM_TRAITS_H_
