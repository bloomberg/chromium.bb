// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_memory.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
#include "ui/gl/gl_surface_egl.h"
#endif

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
      memory_(NULL),
      format_(BufferFormat::RGBA_8888),
      in_use_(false),
      target_(0),
      need_do_bind_tex_image_(false)
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
      ,
      egl_texture_id_(0u),
      egl_image_(EGL_NO_IMAGE_KHR)
#endif
{
}

GLImageMemory::~GLImageMemory() {
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  DCHECK_EQ(EGL_NO_IMAGE_KHR, egl_image_);
  DCHECK_EQ(0u, egl_texture_id_);
#endif
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
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  if (egl_image_ != EGL_NO_IMAGE_KHR) {
    eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(), egl_image_);
    egl_image_ = EGL_NO_IMAGE_KHR;
  }

  if (egl_texture_id_) {
    if (have_context)
      glDeleteTextures(1, &egl_texture_id_);
    egl_texture_id_ = 0u;
  }
#endif
  memory_ = NULL;
}

Size GLImageMemory::GetSize() {
  return size_;
}

unsigned GLImageMemory::GetInternalFormat() {
  return internalformat_;
}

bool GLImageMemory::BindTexImage(unsigned target) {
  if (target_ && target_ != target) {
    LOG(ERROR) << "GLImage can only be bound to one target";
    return false;
  }
  target_ = target;

  // Defer DoBindTexImage if not currently in use.
  if (!in_use_) {
    need_do_bind_tex_image_ = true;
    return true;
  }

  DoBindTexImage(target);
  return true;
}

bool GLImageMemory::CopyTexSubImage(unsigned target,
                                    const Point& offset,
                                    const Rect& rect) {
  TRACE_EVENT2("gpu", "GLImageMemory::CopyTexSubImage", "width", rect.width(),
               "height", rect.height());

  // GL_TEXTURE_EXTERNAL_OES is not a supported CopyTexSubImage target.
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
    glCompressedTexSubImage2D(target,
                              0,  // level
                              offset.x(), offset.y(), rect.width(),
                              rect.height(), DataFormat(format_),
                              SizeInBytes(rect.size(), format_), data);
  } else {
    glTexSubImage2D(target, 0,  // level
                    offset.x(), offset.y(), rect.width(), rect.height(),
                    DataFormat(format_), DataType(format_), data);
  }

  return true;
}

void GLImageMemory::WillUseTexImage() {
  DCHECK(!in_use_);
  in_use_ = true;

  if (!need_do_bind_tex_image_)
    return;

  DCHECK(target_);
  DoBindTexImage(target_);
}

void GLImageMemory::DidUseTexImage() {
  DCHECK(in_use_);
  in_use_ = false;
}

bool GLImageMemory::ScheduleOverlayPlane(AcceleratedWidget widget,
                                         int z_order,
                                         OverlayTransform transform,
                                         const Rect& bounds_rect,
                                         const RectF& crop_rect) {
  return false;
}

void GLImageMemory::DoBindTexImage(unsigned target) {
  TRACE_EVENT0("gpu", "GLImageMemory::DoBindTexImage");

  DCHECK(need_do_bind_tex_image_);
  need_do_bind_tex_image_ = false;

  DCHECK(memory_);
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  if (target == GL_TEXTURE_EXTERNAL_OES) {
    if (egl_image_ == EGL_NO_IMAGE_KHR) {
      DCHECK_EQ(0u, egl_texture_id_);
      glGenTextures(1, &egl_texture_id_);

      {
        ScopedTextureBinder texture_binder(GL_TEXTURE_2D, egl_texture_id_);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (IsCompressedFormat(format_)) {
          glCompressedTexImage2D(GL_TEXTURE_2D,
                                 0,  // mip level
                                 TextureFormat(format_), size_.width(),
                                 size_.height(),
                                 0,  // border
                                 SizeInBytes(size_, format_), memory_);
        } else {
          glTexImage2D(GL_TEXTURE_2D,
                       0,  // mip level
                       TextureFormat(format_),
                       size_.width(),
                       size_.height(),
                       0,  // border
                       DataFormat(format_),
                       DataType(format_),
                       memory_);
        }
      }

      EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
      // Need to pass current EGL rendering context to eglCreateImageKHR for
      // target type EGL_GL_TEXTURE_2D_KHR.
      egl_image_ =
          eglCreateImageKHR(GLSurfaceEGL::GetHardwareDisplay(),
                            eglGetCurrentContext(),
                            EGL_GL_TEXTURE_2D_KHR,
                            reinterpret_cast<EGLClientBuffer>(egl_texture_id_),
                            attrs);
      DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_)
          << "Error creating EGLImage: " << eglGetError();
    } else {
      ScopedTextureBinder texture_binder(GL_TEXTURE_2D, egl_texture_id_);

      if (IsCompressedFormat(format_)) {
        glCompressedTexSubImage2D(GL_TEXTURE_2D,
                                  0,  // mip level
                                  0,  // x-offset
                                  0,  // y-offset
                                  size_.width(), size_.height(),
                                  DataFormat(format_),
                                  SizeInBytes(size_, format_), memory_);
      } else {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,  // mip level
                        0,  // x-offset
                        0,  // y-offset
                        size_.width(),
                        size_.height(),
                        DataFormat(format_),
                        DataType(format_),
                        memory_);
      }
    }

    glEGLImageTargetTexture2DOES(target, egl_image_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    return;
  }
#endif

  DCHECK_NE(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), target);
  if (IsCompressedFormat(format_)) {
    glCompressedTexImage2D(target,
                           0,  // mip level
                           TextureFormat(format_), size_.width(),
                           size_.height(),
                           0,  // border
                           SizeInBytes(size_, format_), memory_);
  } else {
    glTexImage2D(target,
                 0,  // mip level
                 TextureFormat(format_),
                 size_.width(),
                 size_.height(),
                 0,  // border
                 DataFormat(format_),
                 DataType(format_),
                 memory_);
  }
}

void GLImageMemory::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                 uint64_t process_tracing_id,
                                 const std::string& dump_name) {
  // Note that the following calculation does not consider whether this GLImage
  // has been un-bound from a texture. It also relies on this GLImage only ever
  // being bound to a single texture. We could check these conditions more
  // thoroughly, but at the cost of extra GL queries.
  bool is_bound_to_texture = target_ && !need_do_bind_tex_image_;

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  is_bound_to_texture |= !!egl_texture_id_;
#endif

  size_t size_in_bytes = is_bound_to_texture ? SizeInBytes(size_, format_) : 0;

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name + "/texture_memory");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_in_bytes));

  // No need for a global shared edge here. This object in the GPU process is
  // the sole owner of this texture id.
}

}  // namespace gfx
