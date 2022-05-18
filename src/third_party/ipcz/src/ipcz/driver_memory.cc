// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_memory.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

#include "ipcz/ipcz.h"
#include "ipcz/node.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

DriverMemory::DriverMemory() = default;

DriverMemory::DriverMemory(DriverObject memory) : memory_(std::move(memory)) {
  if (memory_.is_valid()) {
    IpczSharedMemoryInfo info = {.size = sizeof(info)};
    IpczResult result = memory_.node()->driver().GetSharedMemoryInfo(
        memory_.handle(), IPCZ_NO_FLAGS, nullptr, &info);
    ABSL_ASSERT(result == IPCZ_RESULT_OK);
    size_ = info.region_num_bytes;
  }
}

DriverMemory::DriverMemory(Ref<Node> node, size_t num_bytes)
    : size_(num_bytes) {
  ABSL_ASSERT(num_bytes > 0);
  IpczDriverHandle handle;
  IpczResult result = node->driver().AllocateSharedMemory(
      num_bytes, IPCZ_NO_FLAGS, nullptr, &handle);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  memory_ = DriverObject(std::move(node), handle);
}

DriverMemory::DriverMemory(DriverMemory&& other) = default;

DriverMemory& DriverMemory::operator=(DriverMemory&& other) = default;

DriverMemory::~DriverMemory() = default;

DriverMemory DriverMemory::Clone() {
  ABSL_ASSERT(is_valid());

  IpczDriverHandle handle;
  IpczResult result = memory_.node()->driver().DuplicateSharedMemory(
      memory_.handle(), 0, nullptr, &handle);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);

  return DriverMemory(DriverObject(memory_.node(), handle));
}

DriverMemoryMapping DriverMemory::Map() {
  ABSL_ASSERT(is_valid());
  void* address;
  IpczDriverHandle mapping_handle;
  IpczResult result = memory_.node()->driver().MapSharedMemory(
      memory_.handle(), 0, nullptr, &address, &mapping_handle);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  return DriverMemoryMapping(memory_.node()->driver(), mapping_handle, address,
                             size_);
}

}  // namespace ipcz
