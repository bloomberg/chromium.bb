// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/mac/foundation_util.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>
#include <Quartz/Quartz.h>

namespace gfx {
namespace {

using WidgetToLayerMap = std::map<AcceleratedWidget, CALayer*>;
base::LazyInstance<WidgetToLayerMap> g_widget_to_layer_map;

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_R8:
    case GL_BGRA_EXT:
    case GL_RGB:
    case GL_RGB_YCBCR_422_CHROMIUM:
      return true;
    default:
      return false;
  }
}

bool ValidFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::BGRA_8888:
    case BufferFormat::UYVY_422:
    case BufferFormat::YUV_420_BIPLANAR:
      return true;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
      return false;
  }

  NOTREACHED();
  return false;
}

GLenum TextureFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::YUV_420_BIPLANAR:
      return GL_RED;
    case BufferFormat::BGRA_8888:
      return GL_RGBA;
    case BufferFormat::UYVY_422:
      return GL_RGB;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::YUV_420_BIPLANAR:
      return GL_RED;
    case BufferFormat::BGRA_8888:
      return GL_BGRA;
    case BufferFormat::UYVY_422:
      return GL_YCBCR_422_APPLE;
      break;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataType(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::YUV_420_BIPLANAR:
      return GL_UNSIGNED_BYTE;
    case BufferFormat::BGRA_8888:
      return GL_UNSIGNED_INT_8_8_8_8_REV;
    case BufferFormat::UYVY_422:
      return GL_UNSIGNED_SHORT_8_8_APPLE;
      break;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

GLImageIOSurface::GLImageIOSurface(const Size& size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat),
      format_(BufferFormat::RGBA_8888) {}

GLImageIOSurface::~GLImageIOSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_surface_);
}

bool GLImageIOSurface::Initialize(IOSurfaceRef io_surface,
                                  GenericSharedMemoryId io_surface_id,
                                  BufferFormat format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_surface_);

  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: " << internalformat_;
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << static_cast<int>(format);
    return false;
  }

  format_ = format;
  io_surface_.reset(io_surface, base::scoped_policy::RETAIN);
  io_surface_id_ = io_surface_id;
  return true;
}

void GLImageIOSurface::Destroy(bool have_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_surface_.reset();
}

Size GLImageIOSurface::GetSize() {
  return size_;
}

unsigned GLImageIOSurface::GetInternalFormat() { return internalformat_; }

bool GLImageIOSurface::BindTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (target != GL_TEXTURE_RECTANGLE_ARB) {
    // This might be supported in the future. For now, perform strict
    // validation so we know what's going on.
    LOG(ERROR) << "IOSurface requires TEXTURE_RECTANGLE_ARB target";
    return false;
  }

  CGLContextObj cgl_context =
      static_cast<CGLContextObj>(GLContext::GetCurrent()->GetHandle());

  DCHECK(io_surface_);
  CGLError cgl_error =
      CGLTexImageIOSurface2D(cgl_context, target, TextureFormat(format_),
                             size_.width(), size_.height(), DataFormat(format_),
                             DataType(format_), io_surface_.get(), 0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "Error in CGLTexImageIOSurface2D";
    return false;
  }

  return true;
}

bool GLImageIOSurface::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageIOSurface::CopyTexSubImage(unsigned target,
                                       const Point& offset,
                                       const Rect& rect) {
  return false;
}

bool GLImageIOSurface::ScheduleOverlayPlane(AcceleratedWidget widget,
                                            int z_order,
                                            OverlayTransform transform,
                                            const Rect& bounds_rect,
                                            const RectF& crop_rect) {
  NOTREACHED();
  return false;
}

void GLImageIOSurface::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                    uint64_t process_tracing_id,
                                    const std::string& dump_name) {
  // IOSurfaceGetAllocSize will return 0 if io_surface_ is invalid. In this case
  // we log 0 for consistency with other GLImage memory dump functions.
  size_t size_bytes = IOSurfaceGetAllocSize(io_surface_);

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_bytes));

  auto guid =
      GetGenericSharedMemoryGUIDForTracing(process_tracing_id, io_surface_id_);
  pmd->CreateSharedGlobalAllocatorDump(guid);
  pmd->AddOwnershipEdge(dump->guid(), guid);
}

base::ScopedCFTypeRef<IOSurfaceRef> GLImageIOSurface::io_surface() {
  return io_surface_;
}

// static
void GLImageIOSurface::SetLayerForWidget(AcceleratedWidget widget,
                                         CALayer* layer) {
  if (layer)
    g_widget_to_layer_map.Pointer()->insert(std::make_pair(widget, layer));
  else
    g_widget_to_layer_map.Pointer()->erase(widget);
}

}  // namespace gfx
