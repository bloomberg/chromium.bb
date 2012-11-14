// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/transport_dib.h"

#include <windows.h>

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::TransportDIB() {
}

TransportDIB::~TransportDIB() {
}

TransportDIB::TransportDIB(HANDLE handle)
    : shared_memory_(handle, false /* read write */) {
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  size_t allocation_granularity = base::SysInfo::VMAllocationGranularity();
  size = size / allocation_granularity + 1;
  size = size * allocation_granularity;

  TransportDIB* dib = new TransportDIB;

  if (!dib->shared_memory_.CreateAnonymous(size)) {
    delete dib;
    return NULL;
  }

  dib->size_ = size;
  dib->sequence_num_ = sequence_num;

  return dib;
}

// static
TransportDIB* TransportDIB::Map(Handle handle) {
  scoped_ptr<TransportDIB> dib(CreateWithHandle(handle));
  if (!dib->Map())
    return NULL;
  return dib.release();
}

// static
TransportDIB* TransportDIB::CreateWithHandle(Handle handle) {
  return new TransportDIB(handle);
}

// static
bool TransportDIB::is_valid_handle(Handle dib) {
  return dib != NULL;
}

// static
bool TransportDIB::is_valid_id(TransportDIB::Id id) {
  return is_valid_handle(id.handle);
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  // This DIB already mapped the file into this process, but PlatformCanvas
  // will map it again.
  DCHECK(!memory()) << "Mapped file twice in the same process.";

  return skia::CreatePlatformCanvas(w, h, true, handle(),
                                    skia::RETURN_NULL_ON_FAILURE);
}

bool TransportDIB::Map() {
  if (!is_valid_handle(handle()))
    return false;
  if (memory())
    return true;

  if (!shared_memory_.Map(0 /* map whole shared memory segment */)) {
    LOG(ERROR) << "Failed to map transport DIB"
               << " handle:" << shared_memory_.handle()
               << " error:" << ::GetLastError();
    return false;
  }

  // There doesn't seem to be any way to find the size of the shared memory
  // region! GetFileSize indicates that the handle is invalid. Thus, we
  // conservatively set the size to the maximum and hope that the renderer
  // isn't about to ask us to read off the end of the array.
  size_ = std::numeric_limits<size_t>::max();
  return true;
}

void* TransportDIB::memory() const {
  return shared_memory_.memory();
}

TransportDIB::Handle TransportDIB::handle() const {
  return shared_memory_.handle();
}

TransportDIB::Id TransportDIB::id() const {
  return Id(handle(), sequence_num_);
}
