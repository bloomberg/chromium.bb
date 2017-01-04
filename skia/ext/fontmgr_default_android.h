// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_FONTMGR_DEFAULT_ANDROID_H_
#define SKIA_EXT_FONTMGR_DEFAULT_ANDROID_H_

#include "third_party/skia/include/core/SkTypes.h"

class SkFontMgr;

SK_API void SetDefaultSkiaFactory(SkFontMgr* fontmgr);

#endif  // SKIA_EXT_FONTMGR_DEFAULT_ANDROID_H_
