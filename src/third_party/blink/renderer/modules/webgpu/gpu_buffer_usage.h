// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_USAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_USAGE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUBufferUsage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_buffer_usage.idl
  static constexpr uint32_t kNone = 0;
  static constexpr uint32_t kMapRead = 1;
  static constexpr uint32_t kMapWrite = 2;
  static constexpr uint32_t kTransferSrc = 4;
  static constexpr uint32_t kTransferDst = 8;
  static constexpr uint32_t kIndex = 16;
  static constexpr uint32_t kVertex = 32;
  static constexpr uint32_t kUniform = 64;
  static constexpr uint32_t kStorage = 128;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUBufferUsage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_USAGE_H_
