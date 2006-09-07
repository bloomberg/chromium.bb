// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// memory_region.h: Access to memory regions.
//
// A MemoryRegion provides virtual access to a range of memory.  It is an
// abstraction allowing the actual source of memory to be independent of
// methods which need to access a virtual memory space.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_MEMORY_REGION_H__
#define PROCESSOR_MEMORY_REGION_H__


#include "google/airbag_types.h"


namespace google_airbag {


class MemoryRegion {
 public:
  virtual ~MemoryRegion() {}

  // The base address of this memory region.
  virtual u_int64_t GetBase() = 0;

  // The size of this memory region.
  virtual u_int32_t GetSize() = 0;

  // Access to data of various sizes within the memory region.  address
  // is a pointer to read, and it must lie within the memory region as
  // defined by its base address and size.  The location pointed to by
  // value is set to the value at address.  Byte-swapping is performed
  // if necessary so that the value is appropriate for the running
  // program.  Returns true on success.  Fails and returns false if address
  // is out of the region's bounds (after considering the width of value),
  // or for other types of errors.
  virtual bool GetMemoryAtAddress(u_int64_t address, u_int8_t*  value) = 0;
  virtual bool GetMemoryAtAddress(u_int64_t address, u_int16_t* value) = 0;
  virtual bool GetMemoryAtAddress(u_int64_t address, u_int32_t* value) = 0;
  virtual bool GetMemoryAtAddress(u_int64_t address, u_int64_t* value) = 0;
};


} // namespace google_airbag


#endif // PROCESSOR_MEMORY_REGION_H__
