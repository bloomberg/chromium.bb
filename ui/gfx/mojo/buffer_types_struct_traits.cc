// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojo/buffer_types_struct_traits.h"

#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::BufferUsageAndFormatDataView,
                  gfx::BufferUsageAndFormat>::
    Read(gfx::mojom::BufferUsageAndFormatDataView data,
         gfx::BufferUsageAndFormat* out) {
  return data.ReadUsage(&out->usage) && data.ReadFormat(&out->format);
}

std::vector<mojo::ScopedHandle>
StructTraits<gfx::mojom::NativePixmapHandleDataView, gfx::NativePixmapHandle>::
    fds(const gfx::NativePixmapHandle& pixmap_handle) {
  std::vector<mojo::ScopedHandle> handles;
#if defined(OS_LINUX)
  for (const base::FileDescriptor& fd : pixmap_handle.fds)
    handles.emplace_back(mojo::WrapPlatformFile(fd.fd));
#endif  // defined(OS_LINUX)
  return handles;
}

bool StructTraits<
    gfx::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::Read(gfx::mojom::NativePixmapHandleDataView data,
                                   gfx::NativePixmapHandle* out) {
#if defined(OS_LINUX)
  mojo::ArrayDataView<mojo::ScopedHandle> handles_data_view;
  data.GetFdsDataView(&handles_data_view);
  for (size_t i = 0; i < handles_data_view.size(); ++i) {
    mojo::ScopedHandle handle = handles_data_view.Take(i);
    base::PlatformFile platform_file;
    if (mojo::UnwrapPlatformFile(std::move(handle), &platform_file) !=
        MOJO_RESULT_OK)
      return false;
    constexpr bool auto_close = true;
    out->fds.push_back(base::FileDescriptor(platform_file, auto_close));
  }
  return data.ReadPlanes(&out->planes);
#else
  return false;
#endif
}

mojo::ScopedSharedBufferHandle
StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
             gfx::GpuMemoryBufferHandle>::
    shared_memory_handle(const gfx::GpuMemoryBufferHandle& handle) {
  if (handle.type != gfx::SHARED_MEMORY_BUFFER &&
      handle.type != gfx::DXGI_SHARED_HANDLE &&
      handle.type != gfx::ANDROID_HARDWARE_BUFFER)
    return mojo::ScopedSharedBufferHandle();
  return mojo::WrapSharedMemoryHandle(
      handle.handle, handle.handle.GetSize(),
      mojo::UnwrappedSharedMemoryHandleProtection::kReadWrite);
}

const gfx::NativePixmapHandle&
StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
             gfx::GpuMemoryBufferHandle>::
    native_pixmap_handle(const gfx::GpuMemoryBufferHandle& handle) {
#if defined(OS_LINUX)
  return handle.native_pixmap_handle;
#else
  static gfx::NativePixmapHandle pixmap_handle;
  return pixmap_handle;
#endif
}

mojo::ScopedHandle StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                                gfx::GpuMemoryBufferHandle>::
    mach_port(const gfx::GpuMemoryBufferHandle& handle) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (handle.type != gfx::IO_SURFACE_BUFFER)
    return mojo::ScopedHandle();
  return mojo::WrapMachPort(handle.mach_port.get());
#else
  return mojo::ScopedHandle();
#endif
}

bool StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                  gfx::GpuMemoryBufferHandle>::
    Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
         gfx::GpuMemoryBufferHandle* out) {
  if (!data.ReadType(&out->type) || !data.ReadId(&out->id))
    return false;

  if (out->type == gfx::SHARED_MEMORY_BUFFER ||
      out->type == gfx::DXGI_SHARED_HANDLE ||
      out->type == gfx::ANDROID_HARDWARE_BUFFER) {
    mojo::ScopedSharedBufferHandle handle = data.TakeSharedMemoryHandle();
    if (handle.is_valid()) {
      MojoResult unwrap_result = mojo::UnwrapSharedMemoryHandle(
          std::move(handle), &out->handle, nullptr, nullptr);
      if (unwrap_result != MOJO_RESULT_OK)
        return false;
    }

    out->offset = data.offset();
    out->stride = data.stride();
  }
#if defined(OS_LINUX)
  if (out->type == gfx::NATIVE_PIXMAP &&
      !data.ReadNativePixmapHandle(&out->native_pixmap_handle))
    return false;
#endif
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (out->type == gfx::IO_SURFACE_BUFFER) {
    mach_port_t mach_port;
    MojoResult unwrap_result =
        mojo::UnwrapMachPort(data.TakeMachPort(), &mach_port);
    if (unwrap_result != MOJO_RESULT_OK)
      return false;
    out->mach_port.reset(mach_port);
  }
#endif
  return true;
}

}  // namespace mojo
