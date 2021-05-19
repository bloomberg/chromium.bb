// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CONTROL_CLIENT_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CONTROL_CLIENT_HOLDER_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace gpu {
namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu
}  // namespace gpu

namespace blink {

// This class holds the WebGPUInterface and a |destroyed_| flag.
// DawnControlClientHolder::Destroy() should be called to destroy the backing
// WebGPUInterface.
class PLATFORM_EXPORT DawnControlClientHolder
    : public RefCounted<DawnControlClientHolder> {
 public:
  DawnControlClientHolder(
      std::unique_ptr<WebGraphicsContext3DProvider> context_provider);

  void Destroy();

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> GetContextProviderWeakPtr()
      const;
  WebGraphicsContext3DProvider* GetContextProvider() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const { return procs_; }
  void SetContextLost();
  bool IsContextLost() const;
  void SetLostContextCallback();

 private:
  friend class RefCounted<DawnControlClientHolder>;
  ~DawnControlClientHolder() = default;

  std::unique_ptr<WebGraphicsContext3DProviderWrapper> context_provider_;
  gpu::webgpu::WebGPUInterface* interface_;
  DawnProcTable procs_;
  bool lost_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_CONTROL_CLIENT_HOLDER_H_
