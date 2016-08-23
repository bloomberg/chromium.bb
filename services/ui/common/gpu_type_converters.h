// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_COMMON_GPU_TYPE_CONVERTERS_H_
#define SERVICES_UI_COMMON_GPU_TYPE_CONVERTERS_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/interfaces/gpu_memory_buffer.mojom.h"

namespace gfx {
struct GpuMemoryBufferHandle;
class GenericSharedMemoryId;
using GpuMemoryBufferId = GenericSharedMemoryId;
struct NativePixmapHandle;
}

namespace mojo {

#if defined(USE_OZONE)
template <>
struct TypeConverter<ui::mojom::NativePixmapHandlePtr,
                     gfx::NativePixmapHandle> {
  static ui::mojom::NativePixmapHandlePtr Convert(
      const gfx::NativePixmapHandle& handle);
};

template <>
struct TypeConverter<gfx::NativePixmapHandle,
                     ui::mojom::NativePixmapHandlePtr> {
  static gfx::NativePixmapHandle Convert(
      const ui::mojom::NativePixmapHandlePtr& handle);
};
#endif

template <>
struct TypeConverter<ui::mojom::GpuMemoryBufferIdPtr, gfx::GpuMemoryBufferId> {
  static ui::mojom::GpuMemoryBufferIdPtr Convert(
      const gfx::GpuMemoryBufferId& id);
};

template <>
struct TypeConverter<gfx::GpuMemoryBufferId, ui::mojom::GpuMemoryBufferIdPtr> {
  static gfx::GpuMemoryBufferId Convert(
      const ui::mojom::GpuMemoryBufferIdPtr& id);
};

template <>
struct TypeConverter<ui::mojom::GpuMemoryBufferHandlePtr,
                     gfx::GpuMemoryBufferHandle> {
  static ui::mojom::GpuMemoryBufferHandlePtr Convert(
      const gfx::GpuMemoryBufferHandle& handle);
};

template <>
struct TypeConverter<gfx::GpuMemoryBufferHandle,
                     ui::mojom::GpuMemoryBufferHandlePtr> {
  static gfx::GpuMemoryBufferHandle Convert(
      const ui::mojom::GpuMemoryBufferHandlePtr& handle);
};

}  // namespace mojo

#endif  // SERVICES_UI_COMMON_GPU_TYPE_CONVERTERS_H_
