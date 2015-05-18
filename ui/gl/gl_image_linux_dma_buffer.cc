// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_linux_dma_buffer.h"

#include <unistd.h>

#define FOURCC(a, b, c, d)                                    \
  ((static_cast<uint32>(a)) | (static_cast<uint32>(b) << 8) | \
   (static_cast<uint32>(c) << 16) | (static_cast<uint32>(d) << 24))

#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')

namespace gfx {
namespace {

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_RGB:
    case GL_BGRA_EXT:
      return true;
    default:
      return false;
  }
}

bool ValidFormat(gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::BGRA_8888:
    case gfx::GpuMemoryBuffer::RGBX_8888:
      return true;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::R_8:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::YUV_420:
      return false;
  }

  NOTREACHED();
  return false;
}

EGLint FourCC(gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case GpuMemoryBuffer::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case GpuMemoryBuffer::RGBX_8888:
      return DRM_FORMAT_XRGB8888;
    case GpuMemoryBuffer::ATC:
    case GpuMemoryBuffer::ATCIA:
    case GpuMemoryBuffer::DXT1:
    case GpuMemoryBuffer::DXT5:
    case GpuMemoryBuffer::ETC1:
    case GpuMemoryBuffer::R_8:
    case GpuMemoryBuffer::RGBA_8888:
    case GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

bool IsHandleValid(const base::FileDescriptor& handle) {
  return handle.fd >= 0;
}

}  // namespace

GLImageLinuxDMABuffer::GLImageLinuxDMABuffer(const Size& size,
                                             unsigned internalformat)
    : GLImageEGL(size), internalformat_(internalformat) {
}

GLImageLinuxDMABuffer::~GLImageLinuxDMABuffer() {
}

bool GLImageLinuxDMABuffer::Initialize(const base::FileDescriptor& handle,
                                       GpuMemoryBuffer::Format format,
                                       int pitch) {
  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: " << internalformat_;
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << format;
    return false;
  }

  if (!IsHandleValid(handle)) {
    LOG(ERROR) << "Invalid file descriptor: " << handle.fd;
    return false;
  }

  // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
  // target, the EGL will take a reference to the dma_buf.
  EGLint attrs[] = {EGL_WIDTH,
                    size_.width(),
                    EGL_HEIGHT,
                    size_.height(),
                    EGL_LINUX_DRM_FOURCC_EXT,
                    FourCC(format),
                    EGL_DMA_BUF_PLANE0_FD_EXT,
                    handle.fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                    0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT,
                    pitch,
                    EGL_NONE};
  return GLImageEGL::Initialize(
      EGL_LINUX_DMA_BUF_EXT, static_cast<EGLClientBuffer>(NULL), attrs);
}

unsigned GLImageLinuxDMABuffer::GetInternalFormat() { return internalformat_; }

}  // namespace gfx
