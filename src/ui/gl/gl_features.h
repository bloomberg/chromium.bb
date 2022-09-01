// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FEATURES_H_
#define UI_GL_GL_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

namespace features {

// Controls if GPU should synchronize presentation with vsync.
GL_EXPORT bool UseGpuVsync();

#if BUILDFLAG(IS_ANDROID)
// Use new Android 13 API to obtain and target a frame deadline.
GL_EXPORT extern const base::Feature kAndroidFrameDeadline;
#endif

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GL_EXPORT extern const base::Feature kDefaultPassthroughCommandDecoder;

GL_EXPORT bool IsAndroidFrameDeadlineEnabled();

GL_EXPORT bool UsePassthroughCommandDecoder();

}  // namespace features

#endif  // UI_GL_GL_FEATURES_H_
