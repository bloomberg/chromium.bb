// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_FEATURES_H_
#define CHROMEOS_UI_WM_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace wm {
namespace features {

// Enable Vertical Snap.
COMPONENT_EXPORT(CHROMEOS_UI_WM)
extern const base::Feature kVerticalSnap;

COMPONENT_EXPORT(CHROMEOS_UI_WM) bool IsVerticalSnapEnabled();

}  // namespace features
}  // namespace wm
}  // namespace chromeos

#endif  // CHROMEOS_UI_WM_FEATURES_H_
