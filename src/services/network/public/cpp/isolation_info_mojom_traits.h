// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ISOLATION_INFO_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ISOLATION_INFO_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/mojom/isolation_info.mojom-shared.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::IsolationInfoRedirectMode,
               net::IsolationInfo::RedirectMode> {
  static network::mojom::IsolationInfoRedirectMode ToMojom(
      net::IsolationInfo::RedirectMode redirect_mode);
  static bool FromMojom(network::mojom::IsolationInfoRedirectMode redirect_mode,
                        net::IsolationInfo::RedirectMode* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::IsolationInfoDataView, net::IsolationInfo> {
  static net::IsolationInfo::RedirectMode redirect_mode(
      const net::IsolationInfo& input) {
    return input.redirect_mode();
  }

  static const base::Optional<url::Origin>& top_frame_origin(
      const net::IsolationInfo& input) {
    return input.top_frame_origin();
  }

  static const base::Optional<url::Origin>& frame_origin(
      const net::IsolationInfo& input) {
    return input.frame_origin();
  }

  static bool opaque_and_non_transient(const net::IsolationInfo& input) {
    return input.opaque_and_non_transient();
  }

  static const net::SiteForCookies& site_for_cookies(
      const net::IsolationInfo& input) {
    return input.site_for_cookies();
  }

  static bool Read(network::mojom::IsolationInfoDataView data,
                   net::IsolationInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ISOLATION_INFO_MOJOM_TRAITS_H_
