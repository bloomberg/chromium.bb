// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_shared_memory.h"

#include "base/logging.h"
#include "base/process/process_handle.h"

namespace gfx {

GLImageSharedMemory::GLImageSharedMemory(const gfx::Size& size,
                                         unsigned internalformat)
    : GLImageMemory(size, internalformat) {
}

GLImageSharedMemory::~GLImageSharedMemory() {
  Destroy();
}

bool GLImageSharedMemory::Initialize(const gfx::GpuMemoryBufferHandle& handle) {
  if (!HasValidFormat())
    return false;

  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return false;

  base::SharedMemory shared_memory(handle.handle, true);

  // Duplicate the handle.
  base::SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                    &duped_shared_memory_handle)) {
    DVLOG(0) << "Failed to duplicate shared memory handle.";
    return false;
  }

  scoped_ptr<base::SharedMemory> duped_shared_memory(
      new base::SharedMemory(duped_shared_memory_handle, true));

  if (!duped_shared_memory->Map(Bytes())) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  DCHECK(!shared_memory_);
  shared_memory_ = duped_shared_memory.Pass();
  GLImageMemory::Initialize(
      static_cast<unsigned char*>(shared_memory_->memory()));
  return true;
}

void GLImageSharedMemory::Destroy() {
  GLImageMemory::Destroy();
  shared_memory_.reset();
}

}  // namespace gfx
