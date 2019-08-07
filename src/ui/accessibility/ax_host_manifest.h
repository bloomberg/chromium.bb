// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_HOST_MANIFEST_H_
#define UI_ACCESSIBILITY_AX_HOST_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace ui {

const service_manager::Manifest& GetAXHostManifest();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_HOST_MANIFEST_H_
