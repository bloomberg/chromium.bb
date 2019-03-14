// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUAdapter;

class GPUDevice final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUDevice* Create(GPUAdapter*);

  GPUDevice(GPUAdapter*);

  GPUAdapter* adapter() const;

  void Trace(blink::Visitor*) override;

 private:
  Member<GPUAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(GPUDevice);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
