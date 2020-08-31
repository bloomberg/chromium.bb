// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser_context_keyed_service_factories.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/platform_apps/api/arc_apps_private/arc_apps_private_api.h"
#endif

namespace chrome_apps {
namespace api {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if defined(OS_CHROMEOS)
  ArcAppsPrivateAPI::GetFactoryInstance();
#endif
}

}  // namespace api
}  // namespace chrome_apps
