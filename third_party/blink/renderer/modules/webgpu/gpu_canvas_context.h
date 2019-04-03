// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUCanvasContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_canvas_context.idl
  // TODO(crbug.com/877147): implement GPUCanvasContext.

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCanvasContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
