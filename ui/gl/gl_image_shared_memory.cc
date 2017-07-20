// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_shared_memory.h"

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_handle.h"
#include "base/sys_info.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer_tracing.h"

namespace gl {

GLImageSharedMemory::GLImageSharedMemory(const gfx::Size& size,
                                         unsigned internalformat)
    : GLImageMemory(size, internalformat) {}

GLImageSharedMemory::~GLImageSharedMemory() {}

bool GLImageSharedMemory::Initialize(
    const base::SharedMemoryHandle& handle,
    gfx::GenericSharedMemoryId shared_memory_id,
    gfx::BufferFormat format,
    size_t offset,
    size_t stride) {
  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, true));

  if (NumberOfPlanesForBufferFormat(format) != 1)
    return false;

  base::CheckedNumeric<size_t> checked_size = stride;
  checked_size *= GetSize().height();
  if (!checked_size.IsValid())
    return false;

  // Minimize the amount of adress space we use but make sure offset is a
  // multiple of page size as required by MapAt().
  size_t memory_offset = offset % base::SysInfo::VMAllocationGranularity();
  size_t map_offset = base::SysInfo::VMAllocationGranularity() *
                      (offset / base::SysInfo::VMAllocationGranularity());

  checked_size += memory_offset;
  if (!checked_size.IsValid())
    return false;

  if (!shared_memory->MapAt(static_cast<off_t>(map_offset),
                            checked_size.ValueOrDie())) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  if (!GLImageMemory::Initialize(
          static_cast<uint8_t*>(shared_memory->memory()) + memory_offset,
          format, stride)) {
    return false;
  }

  DCHECK(!shared_memory_);
  shared_memory_ = std::move(shared_memory);
  shared_memory_id_ = shared_memory_id;
  return true;
}

void GLImageSharedMemory::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  size_t size_in_bytes = 0;

  size_in_bytes = stride() * GetSize().height();

  // Dump under "/shared_memory", as the base class may also dump to
  // "/texture_memory".
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name + "/shared_memory");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_in_bytes));

  auto guid =
      gfx::GetSharedMemoryGUIDForTracing(process_tracing_id, shared_memory_id_);
  auto shared_memory_guid = shared_memory_->mapped_id();
  if (!shared_memory_guid.is_empty()) {
    pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), guid, shared_memory_guid,
                                         0 /* importance */);
  }
}

}  // namespace gl
