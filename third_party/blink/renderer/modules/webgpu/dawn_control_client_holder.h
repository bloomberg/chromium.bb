// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONTROL_CLIENT_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONTROL_CLIENT_HOLDER_H_

#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace gpu {
namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu
}  // namespace gpu

namespace blink {

// This class holds the WebGPUInterface and a |destroyed_| flag.
// DawnControlClientHolder::MarkDestroyed() should be called if the
// backing WebGPUInterface has been destroyed.
class DawnControlClientHolder : public RefCounted<DawnControlClientHolder> {
 public:
  DawnControlClientHolder(gpu::webgpu::WebGPUInterface* interface);

  void MarkDestroyed();
  bool IsDestroyed() const;

  gpu::webgpu::WebGPUInterface* GetInterface() const;

 private:
  friend class RefCounted<DawnControlClientHolder>;
  ~DawnControlClientHolder() = default;

  gpu::webgpu::WebGPUInterface* interface_;
  bool destroyed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONTROL_CLIENT_HOLDER_H_
