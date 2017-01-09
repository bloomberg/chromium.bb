// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RGBAtoRGB_h
#define RGBAtoRGB_h

// TODO(cavalcantii): use regular macro, see https://crbug.com/673067.
#ifdef __ARM_NEON__
#include <arm_neon.h>
#define RGBAtoRGB RGBAtoRGBNeon
#else
#define RGBAtoRGB RGBAtoRGBScalar
#endif

#include "platform/PlatformExport.h"

namespace blink {
PLATFORM_EXPORT void RGBAtoRGBScalar(const unsigned char* pixels,
                                     unsigned pixelCount,
                                     unsigned char* output);
#ifdef __ARM_NEON__
PLATFORM_EXPORT void RGBAtoRGBNeon(const unsigned char* input,
                                   const unsigned pixelCount,
                                   unsigned char* output);
#endif

}  // namespace blink

#endif
