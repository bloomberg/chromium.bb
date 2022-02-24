/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_FLAT_SLICE_GENERATOR_H_
#define SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_FLAT_SLICE_GENERATOR_H_

#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/sqlite/db_sqlite_table.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// Dynamic table generator for "flat slice" table.
//
// The concept of a "flat slice" is to take the data in the slice table and
// remove all notion of nesting; we do this by, at any point in time, taking the
// most specific active slice (i.e. the slice at the bottom of the stack) and
// representing that as the *only* slice that was running during that period.
//
// This concept becomes very useful when you try and linearise a trace and
// compare it with other traces spanning the same user action; "self time" (i.e.
// time spent in a slice but *not* any children) is easily computed and span
// joins with thread state become possible without limiting to only depth zero
// slices.
//
// This table also adds "gap slices" which fill in the gap between top level
// slices with a sentinal values so that comparision of the gap between slices
// is also possible.
//
// As input, this generator takes a start and end timestamp between
// which slices should be picked; we do this rather than just using the trace
// bounds so that the "gap slices" start and end at the appropriate place.
//
// Note that for the start bound we will *not* pick any slice which started
// before the bound even if it finished after. This is dissimilar to span join
// (which picks all slices with ts + dur >= bound) and is more akin to doing
// a simple ts >= bound. However, slices *will* be truncated at the end
// if they would spill past the provided end bound.
class ExperimentalFlatSliceGenerator
    : public DbSqliteTable::DynamicTableGenerator {
 public:
  ExperimentalFlatSliceGenerator(TraceProcessorContext* context);

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  util::Status ValidateConstraints(const QueryConstraints&) override;
  std::unique_ptr<Table> ComputeTable(const std::vector<Constraint>& cs,
                                      const std::vector<Order>& ob,
                                      const BitVector& cols_used) override;

  // Visibile for testing.
  static std::unique_ptr<tables::ExperimentalFlatSliceTable>
  ComputeFlatSliceTable(const tables::SliceTable&,
                        StringPool*,
                        int64_t start_bound,
                        int64_t end_bound);

 private:
  TraceProcessorContext* context_ = nullptr;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_FLAT_SLICE_GENERATOR_H_
