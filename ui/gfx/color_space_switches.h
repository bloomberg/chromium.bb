// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_SWITCHES_H_
#define UI_GFX_COLOR_SPACE_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/gfx/switches_export.h"

namespace features {

GFX_SWITCHES_EXPORT extern const base::Feature kColorCorrectRendering;

}  // namespace features

namespace switches {

GFX_SWITCHES_EXPORT extern const char kForceColorProfile[];

}  // namespace switches

#endif  // UI_GFX_COLOR_SPACE_SWITCHES_H_
