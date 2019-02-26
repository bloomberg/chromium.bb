/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <stdint.h>

#include "perfetto/base/utils.h"
#include "src/profiling/memory/queue_messages.h"
#include "src/profiling/memory/unwinding.h"

namespace perfetto {
namespace profiling {
namespace {

int FuzzUnwinding(const uint8_t* data, size_t size) {
  UnwindingRecord record;
  auto unwinding_metadata = std::make_shared<UnwindingMetadata>(
      getpid(), base::OpenFile("/proc/self/maps", O_RDONLY),
      base::OpenFile("/proc/self/mem", O_RDONLY));

  record.pid = getpid();
  record.size = size;
  record.data.reset(new uint8_t[size]);
  memcpy(record.data.get(), data, size);
  record.metadata = unwinding_metadata;

  BookkeepingRecord out;
  HandleUnwindingRecord(&record, &out);
  return 0;
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return perfetto::profiling::FuzzUnwinding(data, size);
}
