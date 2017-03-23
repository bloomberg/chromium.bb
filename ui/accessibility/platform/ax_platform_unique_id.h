// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_UNIQUE_ID_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_UNIQUE_ID_H_

#include <stdint.h>

#include "ui/accessibility/ax_export.h"

namespace ui {

// Each platform accessibility object has a unique id that's guaranteed to be a
// positive number. (It's stored in an int32_t as opposed to uint32_t because
// some platforms want to negate it, so we want to ensure the range is below the
// signed int max.) This can be used when the id has to be unique across
// multiple frames, since node ids are only unique within one tree.
int32_t AX_EXPORT GetNextAXPlatformNodeUniqueId();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_UNIQUE_ID_H_
