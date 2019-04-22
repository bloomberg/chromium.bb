// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUSwapChain* GPUSwapChain::Create(GPUDevice* device,
                                   DawnSwapChain swap_chain) {
  return MakeGarbageCollected<GPUSwapChain>(device, swap_chain);
}

GPUSwapChain::GPUSwapChain(GPUDevice* device, DawnSwapChain swap_chain)
    : DawnObject<DawnSwapChain>(device, swap_chain) {}

GPUSwapChain::~GPUSwapChain() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().swapChainRelease(GetHandle());
}

}  // namespace blink
