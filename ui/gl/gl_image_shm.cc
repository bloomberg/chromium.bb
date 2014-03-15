// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_shm.h"

#include "base/debug/trace_event.h"
#include "base/process/process_handle.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {

namespace {

bool ValidFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

GLenum TextureFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return GL_BGRA_EXT;
    case GL_RGBA8_OES:
      return GL_RGBA;
    default:
      NOTREACHED();
      return 0;
  }
}

GLenum DataFormat(unsigned internalformat) {
  return TextureFormat(internalformat);
}

GLenum DataType(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return GL_UNSIGNED_BYTE;
    default:
      NOTREACHED();
      return 0;
  }
}

GLenum BytesPerPixel(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return 4;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

GLImageShm::GLImageShm(gfx::Size size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat) {
}

GLImageShm::~GLImageShm() {
  Destroy();
}

bool GLImageShm::Initialize(gfx::GpuMemoryBufferHandle buffer) {
  if (!ValidFormat(internalformat_)) {
    DVLOG(0) << "Invalid format: " << internalformat_;
    return false;
  }

  if (!base::SharedMemory::IsHandleValid(buffer.handle))
    return false;

  base::SharedMemory shared_memory(buffer.handle, true);

  // Duplicate the handle.
  base::SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                    &duped_shared_memory_handle)) {
    DVLOG(0) << "Failed to duplicate shared memory handle.";
    return false;
  }

  shared_memory_.reset(
      new base::SharedMemory(duped_shared_memory_handle, true));
  return true;
}

void GLImageShm::Destroy() {
}

gfx::Size GLImageShm::GetSize() {
  return size_;
}

bool GLImageShm::BindTexImage(unsigned target) {
  TRACE_EVENT0("gpu", "GLImageShm::BindTexImage");
  DCHECK(shared_memory_);
  DCHECK(ValidFormat(internalformat_));

  size_t size = size_.GetArea() * BytesPerPixel(internalformat_);
  DCHECK(!shared_memory_->memory());
  if (!shared_memory_->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  DCHECK(shared_memory_->memory());
  glTexImage2D(target,
               0,  // mip level
               TextureFormat(internalformat_),
               size_.width(),
               size_.height(),
               0,  // border
               DataFormat(internalformat_),
               DataType(internalformat_),
               shared_memory_->memory());

  shared_memory_->Unmap();
  return true;
}

void GLImageShm::ReleaseTexImage(unsigned target) {
}

void GLImageShm::WillUseTexImage() {
}

void GLImageShm::DidUseTexImage() {
}

void GLImageShm::WillModifyTexImage() {
}

void GLImageShm::DidModifyTexImage() {
}

}  // namespace gfx
