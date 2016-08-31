// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/common/gpu_type_converters.h"

#include "build/build_config.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap_handle_ozone.h"
#endif

namespace mojo {

#if defined(USE_OZONE)
// static
ui::mojom::NativePixmapHandlePtr TypeConverter<
    ui::mojom::NativePixmapHandlePtr,
    gfx::NativePixmapHandle>::Convert(const gfx::NativePixmapHandle& handle) {
  // TODO(penghuang); support NativePixmapHandle.
  ui::mojom::NativePixmapHandlePtr result =
      ui::mojom::NativePixmapHandle::New();
  return result;
}

// static
gfx::NativePixmapHandle
TypeConverter<gfx::NativePixmapHandle, ui::mojom::NativePixmapHandlePtr>::
    Convert(const ui::mojom::NativePixmapHandlePtr& handle) {
  // TODO(penghuang); support NativePixmapHandle.
  gfx::NativePixmapHandle result;
  return result;
}
#endif

// static
ui::mojom::GpuMemoryBufferHandlePtr
TypeConverter<ui::mojom::GpuMemoryBufferHandlePtr, gfx::GpuMemoryBufferHandle>::
    Convert(const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER);
  ui::mojom::GpuMemoryBufferHandlePtr result =
      ui::mojom::GpuMemoryBufferHandle::New();
  result->type = handle.type;
  result->id = handle.id;
  base::PlatformFile platform_file;
#if defined(OS_WIN)
  platform_file = handle.handle.GetHandle();
#else
  DCHECK(handle.handle.auto_close || handle.handle.fd == -1);
  platform_file = handle.handle.fd;
#endif
  result->buffer_handle = mojo::WrapPlatformFile(platform_file);
  result->offset = handle.offset;
  result->stride = handle.stride;
#if defined(USE_OZONE)
  result->native_pixmap_handle =
      ui::mojom::NativePixmapHandle::From(handle.native_pixmap_handle);
#endif
  return result;
}

// static
gfx::GpuMemoryBufferHandle
TypeConverter<gfx::GpuMemoryBufferHandle, ui::mojom::GpuMemoryBufferHandlePtr>::
    Convert(const ui::mojom::GpuMemoryBufferHandlePtr& handle) {
  DCHECK_EQ(handle->type, gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER);
  gfx::GpuMemoryBufferHandle result;
  result.type = handle->type;
  result.id = handle->id;
  base::PlatformFile platform_file;
  MojoResult unwrap_result = mojo::UnwrapPlatformFile(
      std::move(handle->buffer_handle), &platform_file);
  if (unwrap_result == MOJO_RESULT_OK) {
#if defined(OS_WIN)
    result.handle =
        base::SharedMemoryHandle(platform_file, base::GetCurrentProcId());
#else
    result.handle = base::SharedMemoryHandle(platform_file, true);
#endif
  }
  result.offset = handle->offset;
  result.stride = handle->stride;
#if defined(USE_OZONE)
  result.native_pixmap_handle =
      handle->native_pixmap_handle.To<gfx::NativePixmapHandle>();
#else
  DCHECK(handle->native_pixmap_handle.is_null());
#endif
  return result;
}

}  // namespace mojo
