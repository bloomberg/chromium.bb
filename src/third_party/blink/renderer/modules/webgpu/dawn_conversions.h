// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONVERSIONS_H_

#include <dawn/dawn.h>

#include <memory>

#include "base/logging.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

// This file provides helpers for converting WebGPU objects, descriptors,
// and enums from Blink to Dawn types.

namespace blink {

class GPUColor;
class GPUExtent3D;
class GPUOrigin3D;
class GPUPipelineStageDescriptor;

// Convert WebGPU bitfield values to Dawn enums. These have the same value.
template <typename DawnEnum>
DawnEnum AsDawnEnum(uint32_t webgpu_enum) {
  return static_cast<DawnEnum>(webgpu_enum);
}

// Convert WebGPU string enums to Dawn enums.
template <typename DawnEnum>
DawnEnum AsDawnEnum(const WTF::String& webgpu_enum);

// These conversions are used multiple times and are declared here. Conversions
// used only once, for example for object construction, are defined
// individually.
DawnColor AsDawnType(const GPUColor*);
DawnExtent3D AsDawnType(const GPUExtent3D*);
DawnOrigin3D AsDawnType(const GPUOrigin3D*);

using OwnedPipelineStageDescriptor =
    std::tuple<DawnPipelineStageDescriptor, std::unique_ptr<char[]>>;
OwnedPipelineStageDescriptor AsDawnType(const GPUPipelineStageDescriptor*);

// WebGPU objects are converted to Dawn objects by getting the opaque handle
// which can be passed to Dawn.
template <typename Handle>
Handle AsDawnType(const DawnObject<Handle>* object) {
  DCHECK(object);
  return object->GetHandle();
}

template <typename WebGPUType>
using TypeOfDawnType = decltype(AsDawnType(std::declval<const WebGPUType*>()));

// Helper for converting a list of objects to Dawn structs or handles
template <typename WebGPUType>
std::unique_ptr<TypeOfDawnType<WebGPUType>[]> AsDawnType(
    const HeapVector<Member<WebGPUType>>& webgpu_objects) {
  using DawnType = TypeOfDawnType<WebGPUType>;

  wtf_size_t count = webgpu_objects.size();
  // TODO(enga): Pass in temporary memory or an allocator so we don't make a
  // separate memory allocation here.
  std::unique_ptr<DawnType[]> dawn_objects(new DawnType[count]);
  for (wtf_size_t i = 0; i < count; ++i) {
    dawn_objects[i] = AsDawnType(webgpu_objects[i].Get());
  }
  return dawn_objects;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_CONVERSIONS_H_
