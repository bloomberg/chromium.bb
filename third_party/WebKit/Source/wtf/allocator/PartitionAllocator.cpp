// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/allocator/PartitionAllocator.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "wtf/allocator/Partitions.h"

namespace WTF {

void* PartitionAllocator::allocateBacking(size_t size, const char* typeName) {
  return Partitions::bufferMalloc(size, typeName);
}

void PartitionAllocator::freeVectorBacking(void* address) {
  Partitions::bufferFree(address);
}

void PartitionAllocator::freeHashTableBacking(void* address) {
  Partitions::bufferFree(address);
}

template <>
char* PartitionAllocator::allocateVectorBacking<char>(size_t size) {
  return reinterpret_cast<char*>(
      allocateBacking(size, "PartitionAllocator::allocateVectorBacking<char>"));
}

template <>
char* PartitionAllocator::allocateExpandedVectorBacking<char>(size_t size) {
  return reinterpret_cast<char*>(allocateBacking(
      size, "PartitionAllocator::allocateExpandedVectorBacking<char>"));
}

}  // namespace WTF
