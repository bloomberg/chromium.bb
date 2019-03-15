// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_control_client_holder.h"

#include "base/logging.h"

namespace blink {

DawnControlClientHolder::DawnControlClientHolder(
    gpu::webgpu::WebGPUInterface* interface)
    : interface_(interface), destroyed_(!interface) {}

void DawnControlClientHolder::MarkDestroyed() {
  destroyed_ = true;
}

bool DawnControlClientHolder::IsDestroyed() const {
  return destroyed_;
}

gpu::webgpu::WebGPUInterface* DawnControlClientHolder::GetInterface() const {
  DCHECK(!destroyed_);
  return interface_;
}

}  // namespace blink
