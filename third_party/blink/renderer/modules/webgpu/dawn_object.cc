// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_control_client_holder.h"

namespace blink {

DawnObject::DawnObject(
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : dawn_control_client_(dawn_control_client) {}

DawnObject::~DawnObject() = default;

const scoped_refptr<DawnControlClientHolder>& DawnObject::GetDawnControlClient()
    const {
  return dawn_control_client_;
}

bool DawnObject::IsDawnControlClientDestroyed() const {
  return dawn_control_client_->IsDestroyed();
}

gpu::webgpu::WebGPUInterface* DawnObject::GetInterface() const {
  return dawn_control_client_->GetInterface();
}

}  // namespace blink
