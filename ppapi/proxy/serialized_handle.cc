// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/serialized_handle.h"

#include "base/pickle.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_NACL)
#include <unistd.h>
#endif

namespace ppapi {
namespace proxy {

SerializedHandle::SerializedHandle()
    : type_(INVALID),
      shm_handle_(base::SharedMemory::NULLHandle()),
      size_(0),
      descriptor_(IPC::InvalidPlatformFileForTransit()) {
}

SerializedHandle::SerializedHandle(Type type_param)
    : type_(type_param),
      shm_handle_(base::SharedMemory::NULLHandle()),
      size_(0),
      descriptor_(IPC::InvalidPlatformFileForTransit()) {
}

SerializedHandle::SerializedHandle(const base::SharedMemoryHandle& handle,
                                   uint32 size)
    : type_(SHARED_MEMORY),
      shm_handle_(handle),
      size_(size),
      descriptor_(IPC::InvalidPlatformFileForTransit()) {
}

SerializedHandle::SerializedHandle(
    Type type,
    const IPC::PlatformFileForTransit& socket_descriptor)
    : type_(type),
      shm_handle_(base::SharedMemory::NULLHandle()),
      size_(0),
      descriptor_(socket_descriptor) {
}

bool SerializedHandle::IsHandleValid() const {
  switch (type_) {
    case SHARED_MEMORY:
      return base::SharedMemory::IsHandleValid(shm_handle_);
    case SOCKET:
    case CHANNEL_HANDLE:
    case FILE:
      return !(IPC::InvalidPlatformFileForTransit() == descriptor_);
    case INVALID:
      return false;
    // No default so the compiler will warn us if a new type is added.
  }
  return false;
}

void SerializedHandle::Close() {
  if (IsHandleValid()) {
    switch (type_) {
      case INVALID:
        NOTREACHED();
        break;
      case SHARED_MEMORY:
        base::SharedMemory::CloseHandle(shm_handle_);
        break;
      case SOCKET:
      case CHANNEL_HANDLE:
      case FILE:
        base::PlatformFile file =
            IPC::PlatformFileForTransitToPlatformFile(descriptor_);
#if !defined(OS_NACL)
        base::ClosePlatformFile(file);
#else
        close(file);
#endif
        break;
      // No default so the compiler will warn us if a new type is added.
    }
  }
  *this = SerializedHandle();
}

// static
bool SerializedHandle::WriteHeader(const Header& hdr, Pickle* pickle) {
  if (!pickle->WriteInt(hdr.type))
    return false;
  if (hdr.type == SHARED_MEMORY) {
    if (!pickle->WriteUInt32(hdr.size))
      return false;
  }
  return true;
}

// static
bool SerializedHandle::ReadHeader(PickleIterator* iter, Header* hdr) {
  *hdr = Header(INVALID, 0);
  int type = 0;
  if (!iter->ReadInt(&type))
    return false;
  bool valid_type = false;
  switch (type) {
    case SHARED_MEMORY: {
      uint32 size = 0;
      if (!iter->ReadUInt32(&size))
        return false;
      hdr->size = size;
      valid_type = true;
      break;
    }
    case SOCKET:
    case CHANNEL_HANDLE:
    case FILE:
    case INVALID:
      valid_type = true;
      break;
    // No default so the compiler will warn us if a new type is added.
  }
  if (valid_type)
    hdr->type = Type(type);
  return valid_type;
}

}  // namespace proxy
}  // namespace ppapi
