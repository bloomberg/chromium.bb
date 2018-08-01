// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_buffer.h"

#include <gbm.h>

#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"

namespace ui {

namespace {

std::unique_ptr<GbmBuffer> CreateBufferForBO(gbm_device* gbm,
                                             struct gbm_bo* bo,
                                             uint32_t format,
                                             const gfx::Size& size,
                                             uint32_t flags) {
  DCHECK(bo);
  std::vector<base::ScopedFD> fds;
  std::vector<gfx::NativePixmapPlane> planes;

  const uint64_t modifier = gbm_bo_get_format_modifier(bo);
  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(gbm_bo_get_plane_fd(bo, i));

    // TODO(dcastagna): support multiple fds.
    // crbug.com/642410
    if (!i) {
      if (!fd.is_valid()) {
        PLOG(ERROR) << "Failed to export buffer to dma_buf";
        gbm_bo_destroy(bo);
        return nullptr;
      }
      fds.emplace_back(std::move(fd));
    }

    planes.emplace_back(gbm_bo_get_plane_stride(bo, i),
                        gbm_bo_get_plane_offset(bo, i),
                        gbm_bo_get_plane_size(bo, i), modifier);
  }
  return std::make_unique<GbmBuffer>(bo, format, flags, modifier,
                                     std::move(fds), size, std::move(planes));
}

}  // namespace

// static
std::unique_ptr<GbmBuffer> GbmBuffer::CreateBufferWithModifiers(
    gbm_device* gbm,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags,
    const std::vector<uint64_t>& modifiers) {
  struct gbm_bo* bo =
      gbm_bo_create_with_modifiers(gbm, size.width(), size.height(), format,
                                   modifiers.data(), modifiers.size());
  if (!bo)
    return nullptr;

  return CreateBufferForBO(gbm, bo, format, size, flags);
}

// static
std::unique_ptr<GbmBuffer> GbmBuffer::CreateBuffer(gbm_device* gbm,
                                                   uint32_t format,
                                                   const gfx::Size& size,
                                                   uint32_t flags) {
  struct gbm_bo* bo =
      gbm_bo_create(gbm, size.width(), size.height(), format, flags);
  if (!bo)
    return nullptr;

  return CreateBufferForBO(gbm, bo, format, size, flags);
}

// static
std::unique_ptr<GbmBuffer> GbmBuffer::CreateBufferFromFds(
    gbm_device* gbm,
    uint32_t format,
    const gfx::Size& size,
    std::vector<base::ScopedFD> fds,
    const std::vector<gfx::NativePixmapPlane>& planes) {
  DCHECK_LE(fds.size(), planes.size());
  DCHECK_EQ(planes[0].offset, 0);

  // Try to use scanout if supported.
  int gbm_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
  if (!gbm_device_is_format_supported(gbm, format, gbm_flags))
    gbm_flags &= ~GBM_BO_USE_SCANOUT;

  struct gbm_bo* bo = nullptr;
  if (gbm_device_is_format_supported(gbm, format, gbm_flags)) {
    struct gbm_import_fd_planar_data fd_data;
    fd_data.width = size.width();
    fd_data.height = size.height();
    fd_data.format = format;

    DCHECK_LE(planes.size(), 3u);
    for (size_t i = 0; i < planes.size(); ++i) {
      fd_data.fds[i] = fds[i < fds.size() ? i : 0].get();
      fd_data.strides[i] = planes[i].stride;
      fd_data.offsets[i] = planes[i].offset;
      fd_data.format_modifiers[i] = planes[i].modifier;
    }

    // The fd passed to gbm_bo_import is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    bo = gbm_bo_import(gbm, GBM_BO_IMPORT_FD_PLANAR, &fd_data, gbm_flags);
    if (!bo) {
      LOG(ERROR) << "nullptr returned from gbm_bo_import";
      return nullptr;
    }
  }

  return std::make_unique<GbmBuffer>(bo, format, gbm_flags, planes[0].modifier,
                                     std::move(fds), size, std::move(planes));
}

GbmBuffer::GbmBuffer(struct gbm_bo* bo,
                     uint32_t format,
                     uint32_t flags,
                     uint64_t modifier,
                     std::vector<base::ScopedFD> fds,
                     const gfx::Size& size,
                     std::vector<gfx::NativePixmapPlane> planes)
    : bo_(bo),
      format_(format),
      format_modifier_(modifier),
      flags_(flags),
      fds_(std::move(fds)),
      size_(size),
      planes_(std::move(planes)) {}

GbmBuffer::~GbmBuffer() {
  gbm_bo_destroy(bo_);
}

gfx::BufferFormat GbmBuffer::GetBufferFormat() const {
  return ui::GetBufferFormatFromFourCCFormat(format_);
}

bool GbmBuffer::AreFdsValid() const {
  if (fds_.empty())
    return false;

  for (const auto& fd : fds_) {
    if (fd.get() == -1)
      return false;
  }
  return true;
}

int GbmBuffer::GetFd(size_t index) const {
  DCHECK_LT(index, fds_.size());
  return fds_[index].get();
}

int GbmBuffer::GetStride(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].stride;
}

int GbmBuffer::GetOffset(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].offset;
}

size_t GbmBuffer::GetPlaneSize(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].size;
}

uint32_t GbmBuffer::GetHandle() const {
  return gbm_bo_get_handle(bo_).u32;
}

gfx::NativePixmapHandle GbmBuffer::ExportHandle() const {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format = ui::GetBufferFormatFromFourCCFormat(format_);
  // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use are
  // supported by gbm.
  for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
    // Some formats (e.g: YVU_420) might have less than one fd per plane.
    if (i < fd_count()) {
      base::ScopedFD scoped_fd(HANDLE_EINTR(dup(GetFd(i))));
      if (!scoped_fd.is_valid()) {
        PLOG(ERROR) << "dup";
        return gfx::NativePixmapHandle();
      }
      handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    }
    handle.planes.emplace_back(GetStride(i), GetOffset(i), GetPlaneSize(i),
                               format_modifier());
  }
  return handle;
}

}  // namespace ui
