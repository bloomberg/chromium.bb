// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONTROL_CLIENT_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONTROL_CLIENT_HOLDER_H_

#include <dawn/dawn.h>

#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
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
  DawnControlClientHolder(
      std::unique_ptr<WebGraphicsContext3DProvider> context_provider);

  void MarkDestroyed();
  bool IsDestroyed() const;

  WebGraphicsContext3DProvider* GetContextProvider() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const;

 private:
  friend class RefCounted<DawnControlClientHolder>;
  ~DawnControlClientHolder() = default;

  std::unique_ptr<WebGraphicsContext3DProvider> context_provider_;
  gpu::webgpu::WebGPUInterface* interface_;
  bool destroyed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONTROL_CLIENT_HOLDER_H_
