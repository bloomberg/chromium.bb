// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_FONTMGR_DEFAULT_FUCHSIA_H_
#define SKIA_EXT_FONTMGR_DEFAULT_FUCHSIA_H_

#include "third_party/skia/include/core/SkTypes.h"

class SkFontMgr;
template <typename T>
class sk_sp;

SK_API void SetDefaultSkiaFactory(sk_sp<SkFontMgr> fontmgr);

#endif  // SKIA_EXT_FONTMGR_DEFAULT_FUCHSIA_H_
