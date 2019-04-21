// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/modules/v8/int32_array_or_long_sequence.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

namespace blink {

class WebGLExtensionScopedContext;
class WebGLMultiDrawCommon {
 protected:
  bool ValidateDrawcount(WebGLExtensionScopedContext* scoped,
                         const char* function_name,
                         GLsizei drawcount);

  bool ValidateArray(WebGLExtensionScopedContext* scoped,
                     const char* function_name,
                     const char* outOfBoundsDescription,
                     size_t size,
                     GLuint offset,
                     GLsizei drawcount);

  static base::span<const int32_t> MakeSpan(
      const Int32ArrayOrLongSequence& array);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_
