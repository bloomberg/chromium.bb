// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_memory.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
namespace {

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_ETC1_RGB8_OES:
    case GL_R8:
    case GL_RGBA:
    case GL_BGRA_EXT:
      return true;
    default:
      return false;
  }
}

bool ValidFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::R_8:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
      return true;
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsCompressedFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
      return true;
    case BufferFormat::R_8:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      return false;
  }

  NOTREACHED();
  return false;
}

GLenum TextureFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::ATC:
      return GL_ATC_RGB_AMD;
    case BufferFormat::ATCIA:
      return GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD;
    case BufferFormat::DXT1:
      return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case BufferFormat::DXT5:
      return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case BufferFormat::ETC1:
      return GL_ETC1_RGB8_OES;
    case BufferFormat::R_8:
      return GL_RED;
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
      return GL_RGBA;
    case BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataFormat(BufferFormat format) {
  return TextureFormat(format);
}

GLenum DataType(BufferFormat format) {
  switch (format) {
    case BufferFormat::RGBA_4444:
      return GL_UNSIGNED_SHORT_4_4_4_4;
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
    case BufferFormat::R_8:
      return GL_UNSIGNED_BYTE;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLsizei SizeInBytes(const Size& size, BufferFormat format) {
  size_t stride_in_bytes = 0;
  bool valid_stride = GLImageMemory::StrideInBytes(
      size.width(), format, &stride_in_bytes);
  DCHECK(valid_stride);
  return static_cast<GLsizei>(stride_in_bytes * size.height());
}

}  // namespace

GLImageMemory::GLImageMemory(const Size& size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat),
      memory_(nullptr),
      format_(BufferFormat::RGBA_8888) {}

GLImageMemory::~GLImageMemory() {
  DCHECK(!memory_);
}

// static
bool GLImageMemory::StrideInBytes(size_t width,
                                  BufferFormat format,
                                  size_t* stride_in_bytes) {
  base::CheckedNumeric<size_t> checked_stride = width;
  switch (format) {
    case BufferFormat::ATCIA:
    case BufferFormat::DXT5:
      *stride_in_bytes = width;
      return true;
    case BufferFormat::ATC:
    case BufferFormat::DXT1:
    case BufferFormat::ETC1:
      DCHECK_EQ(width % 2, 0u);
      *stride_in_bytes = width / 2;
      return true;
    case BufferFormat::R_8:
      checked_stride += 3;
      if (!checked_stride.IsValid())
        return false;
      *stride_in_bytes = checked_stride.ValueOrDie() & ~0x3;
      return true;
    case BufferFormat::RGBA_4444:
      checked_stride *= 2;
      if (!checked_stride.IsValid())
        return false;
      *stride_in_bytes = checked_stride.ValueOrDie();
      return true;
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
      checked_stride *= 4;
      if (!checked_stride.IsValid())
        return false;
      *stride_in_bytes = checked_stride.ValueOrDie();
      return true;
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

bool GLImageMemory::Initialize(const unsigned char* memory,
                               BufferFormat format) {
  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: " << internalformat_;
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << static_cast<int>(format);
    return false;
  }

  DCHECK(memory);
  DCHECK(!memory_);
  DCHECK_IMPLIES(IsCompressedFormat(format), size_.width() % 4 == 0);
  DCHECK_IMPLIES(IsCompressedFormat(format), size_.height() % 4 == 0);
  memory_ = memory;
  format_ = format;
  return true;
}

void GLImageMemory::Destroy(bool have_context) {
  memory_ = nullptr;
}

Size GLImageMemory::GetSize() {
  return size_;
}

unsigned GLImageMemory::GetInternalFormat() {
  return internalformat_;
}

bool GLImageMemory::BindTexImage(unsigned target) {
  return false;
}

bool GLImageMemory::CopyTexImage(unsigned target) {
  TRACE_EVENT2("gpu", "GLImageMemory::CopyTexImage", "width", size_.width(),
               "height", size_.height());

  // GL_TEXTURE_EXTERNAL_OES is not a supported target.
  if (target == GL_TEXTURE_EXTERNAL_OES)
    return false;

  if (IsCompressedFormat(format_)) {
    glCompressedTexImage2D(target, 0, TextureFormat(format_), size_.width(),
                           size_.height(), 0, SizeInBytes(size_, format_),
                           memory_);
  } else {
    glTexImage2D(target, 0, TextureFormat(format_), size_.width(),
                 size_.height(), 0, DataFormat(format_), DataType(format_),
                 memory_);
  }

  return true;
}

bool GLImageMemory::CopyTexSubImage(unsigned target,
                                    const Point& offset,
                                    const Rect& rect) {
  TRACE_EVENT2("gpu", "GLImageMemory::CopyTexSubImage", "width", rect.width(),
               "height", rect.height());

  // GL_TEXTURE_EXTERNAL_OES is not a supported target.
  if (target == GL_TEXTURE_EXTERNAL_OES)
    return false;

  // Sub width is not supported.
  if (rect.width() != size_.width())
    return false;

  // Height must be a multiple of 4 if compressed.
  if (IsCompressedFormat(format_) && rect.height() % 4)
    return false;

  size_t stride_in_bytes = 0;
  bool rv = StrideInBytes(size_.width(), format_, &stride_in_bytes);
  DCHECK(rv);
  DCHECK(memory_);
  const unsigned char* data = memory_ + rect.y() * stride_in_bytes;
  if (IsCompressedFormat(format_)) {
    glCompressedTexSubImage2D(target, 0, offset.x(), offset.y(), rect.width(),
                              rect.height(), DataFormat(format_),
                              SizeInBytes(rect.size(), format_), data);
  } else {
    glTexSubImage2D(target, 0, offset.x(), offset.y(), rect.width(),
                    rect.height(), DataFormat(format_), DataType(format_),
                    data);
  }

  return true;
}

bool GLImageMemory::ScheduleOverlayPlane(AcceleratedWidget widget,
                                         int z_order,
                                         OverlayTransform transform,
                                         const Rect& bounds_rect,
                                         const RectF& crop_rect) {
  return false;
}

}  // namespace gfx
