/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_H_

#include "src/trace_processor/tables/macros.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_STACK_PROFILE_CALLSITE_DEF(NAME, PARENT, C) \
  NAME(StackProfileCallsiteTable, "stack_profile_callsite")     \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                             \
  C(int64_t, depth)                                             \
  C(int64_t, parent_id)                                         \
  C(int64_t, frame_id)

PERFETTO_TP_TABLE(PERFETTO_TP_STACK_PROFILE_CALLSITE_DEF);

#define PERFETTO_TP_SYMBOL_DEF(NAME, PARENT, C) \
  NAME(SymbolTable, "stack_profile_symbol")     \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)             \
  C(uint32_t, symbol_set_id)                    \
  C(StringPool::Id, name)                       \
  C(StringPool::Id, source_file)                \
  C(uint32_t, line_number)

PERFETTO_TP_TABLE(PERFETTO_TP_SYMBOL_DEF);

#define PERFETTO_TP_HEAP_GRAPH_OBJECT_DEF(NAME, PARENT, C)  \
  NAME(HeapGraphObjectTable, "heap_graph_object")           \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                         \
  C(int64_t, upid)                                          \
  C(int64_t, graph_sample_ts)                               \
  C(int64_t, object_id)                                     \
  C(int64_t, self_size)                                     \
  C(int64_t, retained_size)                                 \
  C(int64_t, unique_retained_size)                          \
  C(int64_t, reference_set_id)                              \
  C(int32_t, reachable)                                     \
  C(StringPool::Id, type_name)                              \
  C(base::Optional<StringPool::Id>, deobfuscated_type_name) \
  C(base::Optional<StringPool::Id>, root_type)

PERFETTO_TP_TABLE(PERFETTO_TP_HEAP_GRAPH_OBJECT_DEF);

#define PERFETTO_TP_HEAP_GRAPH_REFERENCE_DEF(NAME, PARENT, C) \
  NAME(HeapGraphReferenceTable, "heap_graph_reference")       \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                           \
  C(int64_t, reference_set_id)                                \
  C(int64_t, owner_id)                                        \
  C(int64_t, owned_id)                                        \
  C(StringPool::Id, field_name)                               \
  C(base::Optional<StringPool::Id>, deobfuscated_field_name)

PERFETTO_TP_TABLE(PERFETTO_TP_HEAP_GRAPH_REFERENCE_DEF);

#define PERFETTO_TP_VULKAN_MEMORY_ALLOCATIONS_DEF(NAME, PARENT, C) \
  NAME(VulkanMemoryAllocationsTable, "vulkan_memory_allocations")  \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                                \
  C(StringPool::Id, source)                                        \
  C(StringPool::Id, operation)                                     \
  C(int64_t, timestamp)                                            \
  C(base::Optional<uint32_t>, upid)                                \
  C(base::Optional<int64_t>, device)                               \
  C(base::Optional<int64_t>, device_memory)                        \
  C(base::Optional<uint32_t>, memory_type)                         \
  C(base::Optional<uint32_t>, heap)                                \
  C(base::Optional<StringPool::Id>, function_name)                 \
  C(base::Optional<int64_t>, object_handle)                        \
  C(base::Optional<int64_t>, memory_address)                       \
  C(base::Optional<int64_t>, memory_size)                          \
  C(StringPool::Id, scope)                                         \
  C(base::Optional<uint32_t>, arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_VULKAN_MEMORY_ALLOCATIONS_DEF);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_H_
