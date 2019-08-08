// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_CORE_ACCOUNT_INFO_MOJOM_TRAITS_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_CORE_ACCOUNT_INFO_MOJOM_TRAITS_H_

#include <string>

#include "components/signin/core/browser/account_info.h"
#include "services/identity/public/mojom/core_account_info.mojom.h"

namespace mojo {

template <>
struct StructTraits<identity::mojom::CoreAccountInfo::DataView,
                    ::CoreAccountInfo> {
  static const CoreAccountId& account_id(const ::CoreAccountInfo& r) {
    return r.account_id;
  }

  static const std::string& gaia(const ::CoreAccountInfo& r) { return r.gaia; }

  static const std::string& email(const ::CoreAccountInfo& r) {
    return r.email;
  }

  static bool Read(identity::mojom::CoreAccountInfo::DataView data,
                   ::CoreAccountInfo* out);

  static bool IsNull(const ::CoreAccountInfo& input);

  static void SetToNull(::CoreAccountInfo* output) {
    *output = CoreAccountInfo();
  }
};

}  // namespace mojo

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_CORE_ACCOUNT_INFO_MOJOM_TRAITS_H_
