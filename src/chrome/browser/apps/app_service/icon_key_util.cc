// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/icon_key_util.h"

namespace apps_util {

IncrementingIconKeyFactory::IncrementingIconKeyFactory() : u_key_(0) {}

apps::mojom::IconKeyPtr IncrementingIconKeyFactory::MakeIconKey(
    apps::mojom::AppType app_type,
    const std::string& s_key,
    uint32_t icon_effects) {
  return apps::mojom::IconKey::New(app_type, ++u_key_, s_key, icon_effects);
}

}  // namespace apps_util
