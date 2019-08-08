// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"

#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

bool ShouldIncludeAppUpdate(const apps::AppUpdate& app_update) {
  // TODO(danan): update this to only return sticky = true arc apps when that
  // attribute is available via the App Service (https://crbug.com/948408).

  return app_update.AppType() == apps::mojom::AppType::kArc;
}
