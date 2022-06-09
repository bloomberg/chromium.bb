// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_WINDOW_INSTANCE_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_WINDOW_INSTANCE_UPDATE_H_

#include "base/unguessable_token.h"

namespace apps {

struct BrowserWindowInstanceUpdate {
  base::UnguessableToken id;
  std::string window_id;
  bool is_active;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_WINDOW_INSTANCE_UPDATE_H_
