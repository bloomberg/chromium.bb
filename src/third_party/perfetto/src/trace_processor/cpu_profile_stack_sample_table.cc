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

#include "src/trace_processor/cpu_profile_stack_sample_table.h"

namespace perfetto {
namespace trace_processor {

CpuProfileStackSampleTable::CpuProfileStackSampleTable(
    sqlite3*,
    const TraceStorage* storage)
    : storage_(storage) {}

void CpuProfileStackSampleTable::RegisterTable(sqlite3* db,
                                               const TraceStorage* storage) {
  SqliteTable::Register<CpuProfileStackSampleTable>(db, storage,
                                                    "cpu_profile_stack_sample");
}

StorageSchema CpuProfileStackSampleTable::CreateStorageSchema() {
  const auto& allocs = storage_->cpu_profile_stack_samples();
  return StorageSchema::Builder()
      .AddGenericNumericColumn("id", RowAccessor())
      .AddOrderedNumericColumn("ts", &allocs.timestamps())
      .AddNumericColumn("callsite_id", &allocs.callsite_ids())
      .AddNumericColumn("utid", &allocs.utids())
      .Build({"id"});
}

uint32_t CpuProfileStackSampleTable::RowCount() {
  return storage_->cpu_profile_stack_samples().size();
}

int CpuProfileStackSampleTable::BestIndex(const QueryConstraints& qc,
                                          BestIndexInfo* info) {
  info->order_by_consumed = true;
  info->estimated_cost = HasEqConstraint(qc, "id") ? 1 : RowCount();
  return SQLITE_OK;
}

}  // namespace trace_processor
}  // namespace perfetto
