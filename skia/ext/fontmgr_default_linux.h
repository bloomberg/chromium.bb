// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_FONTMGR_DEFAULT_LINUX_H_
#define SKIA_EXT_FONTMGR_DEFAULT_LINUX_H_

#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypes.h"

class SkFontMgr;

void SK_API SetDefaultSkiaFactory(sk_sp<SkFontMgr> fontmgr);

#endif  // SKIA_EXT_FONTMGR_DEFAULT_LINUX_H_
