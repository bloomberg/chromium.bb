// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWITCHES_H_
#define UI_GFX_SWITCHES_H_

#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

namespace switches {

#if defined(OS_WIN)
GFX_EXPORT extern const char kDisableDirectWriteForUI[];
#endif

#if defined(OS_MACOSX)
GFX_EXPORT extern const char kEnableHarfBuzzRenderText[];
#endif

GFX_EXPORT extern const char kHeadless[];

GFX_EXPORT extern const char kEnableColorCorrectRendering[];

GFX_EXPORT extern const char kForceColorProfile[];

}  // namespace switches

#endif  // UI_GFX_SWITCHES_H_
