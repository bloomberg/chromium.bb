// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"

#include <fcntl.h>
#include <linux/version.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/memory.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/dma-buf.h>
#else
#include <linux/types.h>

struct dma_buf_sync {
  __u64 flags;
};

#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)

#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)
#endif

namespace gfx {

namespace {

void PrimeSyncStart(int dmabuf_fd) {
  struct dma_buf_sync sync_start = {0};

  sync_start.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
  int rv = HANDLE_EINTR(ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_start));
  PLOG_IF(ERROR, rv) << "Failed DMA_BUF_SYNC_START";
}

void PrimeSyncEnd(int dmabuf_fd) {
  struct dma_buf_sync sync_end = {0};

  sync_end.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
  int rv = HANDLE_EINTR(ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_end));
  PLOG_IF(ERROR, rv) << "Failed DMA_BUF_SYNC_END";
}

}  // namespace

// static
bool ClientNativePixmapDmaBuf::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return format == gfx::BufferFormat::BGR_565 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::BGRX_8888 ||
             format == gfx::BufferFormat::YVU_420;
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::BGRX_8888 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::BGRA_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return
#if defined(ARCH_CPU_X86_FAMILY)
          // Currently only Intel driver (i.e. minigbm and Mesa) supports R_8
          // RG_88, NV12 and XB30. https://crbug.com/356871
          format == gfx::BufferFormat::R_8 ||
          format == gfx::BufferFormat::RG_88 ||
          format == gfx::BufferFormat::YUV_420_BIPLANAR ||
          format == gfx::BufferFormat::RGBX_1010102 ||
#endif

          format == gfx::BufferFormat::BGRX_8888 ||
          format == gfx::BufferFormat::BGRA_8888 ||
          format == gfx::BufferFormat::RGBX_8888 ||
          format == gfx::BufferFormat::RGBA_8888;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return false;

    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return
#if defined(ARCH_CPU_X86_FAMILY)
          // Currently only Intel driver (i.e. minigbm and
          // Mesa) supports R_8 RG_88 and NV12.
          // https://crbug.com/356871
          format == gfx::BufferFormat::R_8 ||
          format == gfx::BufferFormat::RG_88 ||
          format == gfx::BufferFormat::YUV_420_BIPLANAR ||
#endif
          format == gfx::BufferFormat::BGRA_8888;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      // Each platform only supports one camera buffer type. We list the
      // supported buffer formats on all platforms here. When allocating a
      // camera buffer the caller is responsible for making sure a buffer is
      // successfully allocated. For example, allocating YUV420_BIPLANAR
      // for SCANOUT_CAMERA_READ_WRITE may only work on Intel boards.
      return format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      // R_8 is used as the underlying pixel format for BLOB buffers.
      return format == gfx::BufferFormat::R_8;
  }
  NOTREACHED();
  return false;
}

// static
std::unique_ptr<gfx::ClientNativePixmap>
ClientNativePixmapDmaBuf::ImportFromDmabuf(
    const gfx::NativePixmapHandle& handle,
    const gfx::Size& size) {
  return base::WrapUnique(new ClientNativePixmapDmaBuf(handle, size));
}

ClientNativePixmapDmaBuf::ClientNativePixmapDmaBuf(
    const gfx::NativePixmapHandle& handle,
    const gfx::Size& size)
    : pixmap_handle_(handle), size_(size), data_{0} {
  TRACE_EVENT0("drm", "ClientNativePixmapDmaBuf");
  // TODO(dcastagna): support multiple fds.
  DCHECK_EQ(1u, handle.fds.size());
  DCHECK_GE(handle.fds.front().fd, 0);
  dmabuf_fd_.reset(handle.fds.front().fd);

  DCHECK_GE(handle.planes.back().size, 0u);
  size_t map_size = handle.planes.back().offset + handle.planes.back().size;
  data_ = mmap(nullptr, map_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
               dmabuf_fd_.get(), 0);
  if (data_ == MAP_FAILED) {
    logging::SystemErrorCode mmap_error = logging::GetLastSystemErrorCode();
    if (mmap_error == ENOMEM)
      base::TerminateBecauseOutOfMemory(map_size);

    CHECK(false) << "Failed to mmap dmabuf: "
                 << logging::SystemErrorCodeToString(mmap_error);
  }
}

ClientNativePixmapDmaBuf::~ClientNativePixmapDmaBuf() {
  TRACE_EVENT0("drm", "~ClientNativePixmapDmaBuf");
  size_t map_size =
      pixmap_handle_.planes.back().offset + pixmap_handle_.planes.back().size;
  int ret = munmap(data_, map_size);
  DCHECK(!ret);
}

bool ClientNativePixmapDmaBuf::Map() {
  TRACE_EVENT0("drm", "DmaBuf:Map");
  if (data_ != nullptr) {
    PrimeSyncStart(dmabuf_fd_.get());
    return true;
  }
  return false;
}

void ClientNativePixmapDmaBuf::Unmap() {
  TRACE_EVENT0("drm", "DmaBuf:Unmap");
  PrimeSyncEnd(dmabuf_fd_.get());
}

void* ClientNativePixmapDmaBuf::GetMemoryAddress(size_t plane) const {
  DCHECK_LT(plane, pixmap_handle_.planes.size());
  uint8_t* address = reinterpret_cast<uint8_t*>(data_);
  return address + pixmap_handle_.planes[plane].offset;
}

int ClientNativePixmapDmaBuf::GetStride(size_t plane) const {
  DCHECK_LT(plane, pixmap_handle_.planes.size());
  return pixmap_handle_.planes[plane].stride;
}

}  // namespace gfx
