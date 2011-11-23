// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_SANDBOX_SUPPORT_WIN_H_
#define SKIA_EXT_SKIA_SANDBOX_SUPPORT_WIN_H_
#pragma once

#include <windows.h>

#include "SkPreConfig.h"

typedef void (*SkiaEnsureTypefaceAccessible)(LOGFONT font);

SK_API void SetSkiaEnsureTypefaceAccessible(SkiaEnsureTypefaceAccessible func);

#endif  // SKIA_EXT_SKIA_SANDBOX_SUPPORT_WIN_H_
