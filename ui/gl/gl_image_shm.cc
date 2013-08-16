// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_shm.h"

#include "base/debug/trace_event.h"
#include "base/process/process_handle.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {

GLImageShm::GLImageShm(gfx::Size size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat) {
  // GL_RGBA8_OES is currently the only supported internalformat.
  DCHECK_EQ(static_cast<GLenum>(GL_RGBA8_OES), internalformat);
}

GLImageShm::~GLImageShm() {
  Destroy();
}

bool GLImageShm::Initialize(gfx::GpuMemoryBufferHandle buffer) {
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

bool GLImageShm::BindTexImage() {
  TRACE_EVENT0("gpu", "GLImageShm::BindTexImage");
  DCHECK(shared_memory_);

  GLenum internalformat;
  GLenum format;
  GLenum type;
  int bytes_per_pixel;
  switch (internalformat_) {
    case GL_RGBA8_OES:
      internalformat = GL_RGBA;
      format = GL_RGBA;
      type = GL_UNSIGNED_BYTE;
      bytes_per_pixel = 4;
      break;
    default:
      DVLOG(0) << "Invalid format: " << internalformat_;
      return false;
  }

  size_t size = size_.GetArea() * bytes_per_pixel;
  DCHECK(!shared_memory_->memory());
  if (!shared_memory_->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  DCHECK(shared_memory_->memory());
  glTexImage2D(GL_TEXTURE_2D,
               0,  // mip level
               internalformat,
               size_.width(),
               size_.height(),
               0,  // border
               format,
               type,
               shared_memory_->memory());

  shared_memory_->Unmap();
  return true;
}

gfx::Size GLImageShm::GetSize() {
  return size_;
}

void GLImageShm::Destroy() {
}

void GLImageShm::ReleaseTexImage() {
}

}  // namespace gfx
