// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_

#include <dawn/dawn.h>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace gpu {
namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu
}  // namespace gpu

namespace blink {

class GPUDevice;
class Visitor;

// This class allows objects to hold onto a DawnControlClientHolder.
// The DawnControlClientHolder is used to hold the WebGPUInterface and keep
// track of whether or not the client has been destroyed. If the client is
// destroyed, we should not call any Dawn functions.
class DawnObjectBase : public ScriptWrappable {
 public:
  DawnObjectBase(scoped_refptr<DawnControlClientHolder> dawn_control_client);
  ~DawnObjectBase() override;

  const scoped_refptr<DawnControlClientHolder>& GetDawnControlClient() const;
  bool IsDawnControlClientDestroyed() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const;

 private:
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
};

class DawnObjectImpl : public DawnObjectBase {
 public:
  DawnObjectImpl(GPUDevice* device);

  void Trace(blink::Visitor* visitor) override;

 protected:
  Member<GPUDevice> device_;
};

template <typename Handle>
class DawnObject : public DawnObjectImpl {
 public:
  DawnObject(GPUDevice* device, Handle handle)
      : DawnObjectImpl(device), handle_(handle) {}

  Handle GetHandle() const { return handle_; }

 private:
  Handle const handle_;
};

template <>
class DawnObject<DawnDevice> : public DawnObjectBase {
 public:
  DawnObject(scoped_refptr<DawnControlClientHolder> dawn_control_client,
             DawnDevice handle)
      : DawnObjectBase(std::move(dawn_control_client)), handle_(handle) {}

  DawnDevice GetHandle() const { return handle_; }

 private:
  DawnDevice const handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
