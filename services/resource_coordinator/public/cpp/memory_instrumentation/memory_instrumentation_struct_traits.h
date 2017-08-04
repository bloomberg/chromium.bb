// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_STRUCT_TRAITS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_STRUCT_TRAITS_H_

#include "base/process/process_handle.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_export.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace mojo {

template <>
struct SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT
    EnumTraits<memory_instrumentation::mojom::DumpType,
               base::trace_event::MemoryDumpType> {
  static memory_instrumentation::mojom::DumpType ToMojom(
      base::trace_event::MemoryDumpType type);
  static bool FromMojom(memory_instrumentation::mojom::DumpType input,
                        base::trace_event::MemoryDumpType* out);
};

template <>
struct SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT
    EnumTraits<memory_instrumentation::mojom::LevelOfDetail,
               base::trace_event::MemoryDumpLevelOfDetail> {
  static memory_instrumentation::mojom::LevelOfDetail ToMojom(
      base::trace_event::MemoryDumpLevelOfDetail level_of_detail);
  static bool FromMojom(memory_instrumentation::mojom::LevelOfDetail input,
                        base::trace_event::MemoryDumpLevelOfDetail* out);
};

template <>
struct SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT
    StructTraits<memory_instrumentation::mojom::RequestArgsDataView,
                 base::trace_event::MemoryDumpRequestArgs> {
  static uint64_t dump_guid(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.dump_guid;
  }
  static base::trace_event::MemoryDumpType dump_type(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.dump_type;
  }
  static base::trace_event::MemoryDumpLevelOfDetail level_of_detail(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.level_of_detail;
  }
  static bool Read(memory_instrumentation::mojom::RequestArgsDataView input,
                   base::trace_event::MemoryDumpRequestArgs* out);
};

}  // namespace mojo

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_STRUCT_TRAITS_H_
