// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_wrapper.h"

#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

#include "base/posix/eintr_wrapper.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/common/linux/gbm_device.h"

namespace gbm_wrapper {

namespace {

// Minigbm and system linux gbm have some differences. There is no clear way how
// to distinguish linux gbm (Mesa, basically) and minigbm, which can be both
// third_party and minigbm. Thus, use GBM_BO_IMPORT_FD_PLANAR define to
// identify, which gbm is used.
#if defined(GBM_BO_IMPORT_FD_PLANAR)
// Minigbm defines GBM_BO_IMPORT_FD_PLANAR, which is unknown in linux gbm.
// Redefine it in a common define.
#define GBM_BO_IMPORT_FD_DATA GBM_BO_IMPORT_FD_PLANAR

// There are some methods, which require knowing whether minigbm is used or not.
#ifndef USING_MINIGBM
#define USING_MINIGBM
#endif  // USING_MINIGBM

// Minigbm and system linux gbm have alike gbm_bo_import* structures, but some
// of the data variables have different type.
using gbm_bo_import_fd_data_with_modifier = struct gbm_import_fd_planar_data;
#else
// See comment above. Linux defines GBM_BO_IMPORT_FD_MODIFIER, thus, redefine it
// in a common define.
#define GBM_BO_IMPORT_FD_DATA GBM_BO_IMPORT_FD_MODIFIER
// See comment above.
using gbm_bo_import_fd_data_with_modifier = struct gbm_import_fd_modifier_data;
#endif

void InitializeImportData(uint32_t format,
                          const gfx::Size& size,
                          const std::vector<base::ScopedFD>& fds,
                          const std::vector<gfx::NativePixmapPlane>& planes,
                          gbm_bo_import_fd_data_with_modifier* fd_data) {
  fd_data->width = size.width();
  fd_data->height = size.height();
  fd_data->format = format;

  DCHECK_LE(planes.size(), 3u);
  for (size_t i = 0; i < planes.size(); ++i) {
    fd_data->fds[i] = fds[i < fds.size() ? i : 0].get();
    fd_data->strides[i] = planes[i].stride;
    fd_data->offsets[i] = planes[i].offset;
#if defined(USING_MINIGBM)
    fd_data->format_modifiers[i] = planes[i].modifier;
#else
    fd_data->modifier = planes[i].modifier;
#endif
  }
}

int GetPlaneFdForBo(gbm_bo* bo, size_t plane) {
  DCHECK(plane < gbm_bo_get_plane_count(bo));

  // System linux gbm (or Mesa gbm) does not provide fds per plane basis. Thus,
  // get plane handle and use drm ioctl to get a prime fd out of it avoid having
  // two different branches for minigbm and Mesa gbm here.
  gbm_device* gbm_dev = gbm_bo_get_device(bo);
  int dev_fd = gbm_device_get_fd(gbm_dev);
  if (dev_fd <= 0) {
    LOG(ERROR) << "Unable to get device fd";
    return -1;
  }

  const uint32_t plane_handle = gbm_bo_get_handle_for_plane(bo, plane).u32;
  int fd = -1;
  int ret;
  // Use DRM_RDWR to allow the fd to be mappable in another process.
  ret = drmPrimeHandleToFD(dev_fd, plane_handle, DRM_CLOEXEC | DRM_RDWR, &fd);

  // Older DRM implementations blocked DRM_RDWR, but gave a read/write mapping
  // anyways
  if (ret)
    ret = drmPrimeHandleToFD(dev_fd, plane_handle, DRM_CLOEXEC, &fd);

  return ret ? ret : fd;
}

size_t GetSizeOfPlane(gbm_bo* bo, size_t plane) {
  // System linux gbm (or Mesa gbm) does not provide plane size. Thus, calculate
  // it by ourselves and avoid having two different branches for minigbm and
  // Mesa gbm here.
  return gbm_bo_get_height(bo) * gbm_bo_get_stride_for_plane(bo, plane);
}

}  // namespace

class Buffer final : public ui::GbmBuffer {
 public:
  Buffer(struct gbm_bo* bo,
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

  ~Buffer() override {
    DCHECK(!mmap_data_);
    gbm_bo_destroy(bo_);
  }

  uint32_t GetFormat() const override { return format_; }
  uint64_t GetFormatModifier() const override { return format_modifier_; }
  uint32_t GetFlags() const override { return flags_; }
  size_t GetFdCount() const override { return fds_.size(); }
  // TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
  // as the size would be queried directly from the underlying bo.
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetBufferFormat() const override {
    return ui::GetBufferFormatFromFourCCFormat(format_);
  }
  bool AreFdsValid() const override {
    if (fds_.empty())
      return false;

    for (const auto& fd : fds_) {
      if (fd.get() == -1)
        return false;
    }
    return true;
  }
  size_t GetNumPlanes() const override { return planes_.size(); }
  int GetPlaneFd(size_t plane) const override {
    DCHECK_LT(plane, fds_.size());
    return fds_[plane].get();
  }
  int GetPlaneStride(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].stride;
  }
  int GetPlaneOffset(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].offset;
  }
  size_t GetPlaneSize(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].size;
  }
  uint32_t GetPlaneHandle(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return gbm_bo_get_handle_for_plane(bo_, plane).u32;
  }
  uint32_t GetHandle() const override { return gbm_bo_get_handle(bo_).u32; }
  gfx::NativePixmapHandle ExportHandle() const override {
    gfx::NativePixmapHandle handle;
    gfx::BufferFormat format = ui::GetBufferFormatFromFourCCFormat(format_);
    // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use
    // are supported by gbm.
    for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
      // Some formats (e.g: YVU_420) might have less than one fd per plane.
      if (i < fds_.size()) {
        base::ScopedFD scoped_fd(HANDLE_EINTR(dup(GetPlaneFd(i))));
        if (!scoped_fd.is_valid()) {
          PLOG(ERROR) << "dup";
          return gfx::NativePixmapHandle();
        }
        handle.fds.emplace_back(
            base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
      }
      handle.planes.emplace_back(GetPlaneStride(i), GetPlaneOffset(i),
                                 GetPlaneSize(i), GetFormatModifier());
    }
    return handle;
  }

  sk_sp<SkSurface> GetSurface() override {
    DCHECK(!mmap_data_);
    uint32_t stride;
    void* addr;
    addr = gbm_bo_map(bo_, 0, 0, gbm_bo_get_width(bo_), gbm_bo_get_height(bo_),
                      GBM_BO_TRANSFER_READ_WRITE, &stride, &mmap_data_, 0);

    if (!addr)
      return nullptr;
    SkImageInfo info =
        SkImageInfo::MakeN32Premul(size_.width(), size_.height());
    return SkSurface::MakeRasterDirectReleaseProc(info, addr, stride,
                                                  &Buffer::UnmapGbmBo, this);
  }

 private:
  gbm_bo* bo_ = nullptr;
  void* mmap_data_ = nullptr;

  uint32_t format_ = 0;
  uint64_t format_modifier_ = 0;
  uint32_t flags_ = 0;

  std::vector<base::ScopedFD> fds_;

  gfx::Size size_;

  std::vector<gfx::NativePixmapPlane> planes_;

  static void UnmapGbmBo(void* pixels, void* context) {
    Buffer* buffer = static_cast<Buffer*>(context);
    gbm_bo_unmap(buffer->bo_, buffer->mmap_data_);
    buffer->mmap_data_ = nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

std::unique_ptr<Buffer> CreateBufferForBO(struct gbm_bo* bo,
                                          uint32_t format,
                                          const gfx::Size& size,
                                          uint32_t flags) {
  DCHECK(bo);
  std::vector<base::ScopedFD> fds;
  std::vector<gfx::NativePixmapPlane> planes;

  const uint64_t modifier = gbm_bo_get_modifier(bo);
  const int plane_count = gbm_bo_get_plane_count(bo);
  // The Mesa's gbm implementation explicitly checks whether plane count <= and
  // returns 1 if the condition is true. Nevertheless, use a DCHECK here to make
  // sure the condition is not broken there.
  DCHECK(plane_count > 0);
  // Ensure there are no differences in integer signs by casting any possible
  // values to size_t.
  for (size_t i = 0; i < static_cast<size_t>(plane_count); ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(GetPlaneFdForBo(bo, i));

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

    planes.emplace_back(gbm_bo_get_stride_for_plane(bo, i),
                        gbm_bo_get_offset(bo, i), GetSizeOfPlane(bo, i),
                        modifier);
  }
  return std::make_unique<Buffer>(bo, format, flags, modifier, std::move(fds),
                                  size, std::move(planes));
}

class Device final : public ui::GbmDevice {
 public:
  Device(gbm_device* device) : device_(device) {}
  ~Device() override { gbm_device_destroy(device_); }

  std::unique_ptr<ui::GbmBuffer> CreateBuffer(uint32_t format,
                                              const gfx::Size& size,
                                              uint32_t flags) override {
    struct gbm_bo* bo =
        gbm_bo_create(device_, size.width(), size.height(), format, flags);
    if (!bo)
      return nullptr;

    return CreateBufferForBO(bo, format, size, flags);
  }

  std::unique_ptr<ui::GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) override {
    if (modifiers.empty())
      return CreateBuffer(format, size, flags);
    struct gbm_bo* bo = gbm_bo_create_with_modifiers(
        device_, size.width(), size.height(), format, modifiers.data(),
        modifiers.size());
    if (!bo)
      return nullptr;

    return CreateBufferForBO(bo, format, size, flags);
  }

  std::unique_ptr<ui::GbmBuffer> CreateBufferFromFds(
      uint32_t format,
      const gfx::Size& size,
      std::vector<base::ScopedFD> fds,
      const std::vector<gfx::NativePixmapPlane>& planes) override {
    DCHECK_LE(fds.size(), planes.size());
    DCHECK_EQ(planes[0].offset, 0);

    // Try to use scanout if supported.
    int gbm_flags = GBM_BO_USE_SCANOUT;
#if defined(GBM_BO_USE_TEXTURING)
    gbm_flags |= GBM_BO_USE_TEXTURING;
#endif
    if (!gbm_device_is_format_supported(device_, format, gbm_flags))
      gbm_flags &= ~GBM_BO_USE_SCANOUT;

    struct gbm_bo* bo = nullptr;
    if (gbm_device_is_format_supported(device_, format, gbm_flags)) {
      gbm_bo_import_fd_data_with_modifier fd_data;
      InitializeImportData(format, size, fds, planes, &fd_data);

      // The fd passed to gbm_bo_import is not ref-counted and need to be
      // kept open for the lifetime of the buffer.
      bo = gbm_bo_import(device_, GBM_BO_IMPORT_FD_DATA, &fd_data, gbm_flags);
      if (!bo) {
        LOG(ERROR) << "nullptr returned from gbm_bo_import";
        return nullptr;
      }
    }

    return std::make_unique<Buffer>(bo, format, gbm_flags, planes[0].modifier,
                                    std::move(fds), size, std::move(planes));
  }

 private:
  gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace gbm_wrapper

namespace ui {

std::unique_ptr<GbmDevice> CreateGbmDevice(int fd) {
  gbm_device* device = gbm_create_device(fd);
  if (!device)
    return nullptr;
  return std::make_unique<gbm_wrapper::Device>(device);
}

}  // namespace ui
