// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/common/gpu_type_converters.h"

#include "build/build_config.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_channel_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap_handle_ozone.h"
#endif

namespace mojo {

// static
ui::mojom::ChannelHandlePtr
TypeConverter<ui::mojom::ChannelHandlePtr, IPC::ChannelHandle>::Convert(
    const IPC::ChannelHandle& handle) {
  ui::mojom::ChannelHandlePtr result = ui::mojom::ChannelHandle::New();
  result->name = handle.name;
#if defined(OS_WIN)
  // On windows, a pipe handle Will NOT be marshalled over IPC.
  DCHECK(handle.pipe.handle == NULL);
#else
  DCHECK(handle.socket.auto_close || handle.socket.fd == -1);
  base::PlatformFile platform_file = handle.socket.fd;
  if (platform_file != -1)
    result->socket = mojo::WrapPlatformFile(platform_file);
#endif
  result->mojo_handle.reset(handle.mojo_handle);
  return result;
}

// static
IPC::ChannelHandle
TypeConverter<IPC::ChannelHandle, ui::mojom::ChannelHandlePtr>::Convert(
    const ui::mojom::ChannelHandlePtr& handle) {
  if (handle.is_null())
    return IPC::ChannelHandle();
  if (handle->mojo_handle.is_valid()) {
    IPC::ChannelHandle channel_handle(handle->mojo_handle.release());
    channel_handle.name = handle->name;
    return channel_handle;
  }
#if defined(OS_WIN)
  // On windows, a pipe handle Will NOT be marshalled over IPC.
  DCHECK(!handle->socket.is_valid());
  return IPC::ChannelHandle(handle->name);
#else
  base::PlatformFile platform_file = -1;
  mojo::UnwrapPlatformFile(std::move(handle->socket), &platform_file);
  return IPC::ChannelHandle(handle->name,
                            base::FileDescriptor(platform_file, true));
#endif
}

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
ui::mojom::GpuMemoryBufferIdPtr
TypeConverter<ui::mojom::GpuMemoryBufferIdPtr, gfx::GpuMemoryBufferId>::Convert(
    const gfx::GpuMemoryBufferId& id) {
  ui::mojom::GpuMemoryBufferIdPtr result = ui::mojom::GpuMemoryBufferId::New();
  result->id = id.id;
  return result;
}

// static
gfx::GpuMemoryBufferId
TypeConverter<gfx::GpuMemoryBufferId, ui::mojom::GpuMemoryBufferIdPtr>::Convert(
    const ui::mojom::GpuMemoryBufferIdPtr& id) {
  return gfx::GpuMemoryBufferId(id->id);
}

// static
ui::mojom::GpuMemoryBufferHandlePtr
TypeConverter<ui::mojom::GpuMemoryBufferHandlePtr, gfx::GpuMemoryBufferHandle>::
    Convert(const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER);
  ui::mojom::GpuMemoryBufferHandlePtr result =
      ui::mojom::GpuMemoryBufferHandle::New();
  result->type = static_cast<ui::mojom::GpuMemoryBufferType>(handle.type);
  result->id = ui::mojom::GpuMemoryBufferId::From(handle.id);
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
  DCHECK(handle->type == ui::mojom::GpuMemoryBufferType::SHARED_MEMORY);
  gfx::GpuMemoryBufferHandle result;
  result.type = static_cast<gfx::GpuMemoryBufferType>(handle->type);
  result.id = handle->id.To<gfx::GpuMemoryBufferId>();
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

// static
ui::mojom::GpuInfoPtr
TypeConverter<ui::mojom::GpuInfoPtr, gpu::GPUInfo>::Convert(
    const gpu::GPUInfo& input) {
  ui::mojom::GpuInfoPtr result(ui::mojom::GpuInfo::New());
  result->vendor_id = input.gpu.vendor_id;
  result->device_id = input.gpu.device_id;
  result->vendor_info = mojo::String::From<std::string>(input.gl_vendor);
  result->renderer_info = mojo::String::From<std::string>(input.gl_renderer);
  result->driver_version =
      mojo::String::From<std::string>(input.driver_version);
  return result;
}

}  // namespace mojo
