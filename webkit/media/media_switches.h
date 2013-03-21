// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_MEDIA_SWITCHES_H_
#define WEBKIT_MEDIA_MEDIA_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

#if defined(OS_ANDROID)
extern const char kUseExternalVideoSurface[];
#endif

}  // namespace switches

#endif  // WEBKIT_MEDIA_MEDIA_SWITCHES_H_
