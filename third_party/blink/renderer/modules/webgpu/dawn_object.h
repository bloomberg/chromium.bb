// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace gpu {
namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu
}  // namespace gpu

namespace blink {

class DawnControlClientHolder;

// This class allows objects to hold onto a DawnControlClientHolder.
// The DawnControlClientHolder is used to hold the WebGPUInterface and keep
// track of whether or not the client has been destroyed. If the client is
// destroyed, we should not call any Dawn functions.
class DawnObject : public ScriptWrappable {
 public:
  DawnObject(scoped_refptr<DawnControlClientHolder> dawn_control_client);
  ~DawnObject() override;

  const scoped_refptr<DawnControlClientHolder>& GetDawnControlClient() const;
  bool IsDawnControlClientDestroyed() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;

 private:
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
