// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/core_account_info_mojom_traits.h"

#include "services/identity/public/cpp/core_account_id_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    identity::mojom::CoreAccountInfo::DataView,
    ::CoreAccountInfo>::Read(identity::mojom::CoreAccountInfo::DataView data,
                             ::CoreAccountInfo* out) {
  CoreAccountId account_id;
  std::string gaia;
  std::string email;

  if (!data.ReadAccountId(&account_id) || !data.ReadGaia(&gaia) ||
      !data.ReadEmail(&email)) {
    return false;
  }

  out->account_id = account_id;
  out->gaia = gaia;
  out->email = email;

  return true;
}

// static
bool StructTraits<identity::mojom::CoreAccountInfo::DataView,
                  ::CoreAccountInfo>::IsNull(const ::CoreAccountInfo& input) {
  // Note that a CoreAccountInfo being null cannot be defined as
  // CoreAccountInfo::IsEmpty(), as IsEmpty() verifies that *all* fields are
  // empty, which is not enough to ensure that only valid (i.e. fully filled)
  // CoreAccountInfo are passed.
  return input.account_id.empty() || input.gaia.empty() || input.email.empty();
}

}  // namespace mojo
