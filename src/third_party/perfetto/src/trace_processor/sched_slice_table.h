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

#ifndef SRC_TRACE_PROCESSOR_SCHED_SLICE_TABLE_H_
#define SRC_TRACE_PROCESSOR_SCHED_SLICE_TABLE_H_

#include <sqlite3.h>
#include <limits>
#include <memory>

#include "perfetto/base/utils.h"
#include "src/trace_processor/query_constraints.h"
#include "src/trace_processor/table.h"
#include "src/trace_processor/trace_storage.h"

namespace perfetto {
namespace trace_processor {

// The implementation of the SQLite table containing slices of CPU time with the
// metadata for those slices.
class SchedSliceTable : public Table {
 public:
  enum Column {
    kQuantum = 0,
    kTimestampLowerBound = 1,
    kTimestamp = 2,
    kCpu = 3,
    kDuration = 4,
    kQuantizedGroup = 5,
    kUtid = 6
  };

  SchedSliceTable(const TraceStorage* storage);

  static void RegisterTable(sqlite3* db, const TraceStorage* storage);

  // Table implementation.
  std::unique_ptr<Table::Cursor> CreateCursor() override;
  int BestIndex(const QueryConstraints&, BestIndexInfo*) override;
  int FindFunction(const char* name, FindFunctionFn fn, void** args) override;

 private:
  // Transient filter state for each CPU of this trace.
  class PerCpuState {
   public:
    void Initialize(uint32_t cpu,
                    const TraceStorage* storage,
                    uint64_t quantum,
                    std::vector<uint32_t> sorted_row_ids);
    void FindNextSlice();
    bool IsNextRowIdIndexValid() const {
      return next_row_id_index_ < sorted_row_ids_.size();
    }

    size_t next_row_id() const { return sorted_row_ids_[next_row_id_index_]; }
    uint64_t next_timestamp() const { return next_timestamp_; }

   private:
    const TraceStorage::SlicesPerCpu& Slices() {
      return storage_->SlicesForCpu(cpu_);
    }

    void UpdateNextTimestampForNextRow();

    // Vector of row ids sorted by the the given order by constraints.
    std::vector<uint32_t> sorted_row_ids_;

    // An offset into |sorted_row_ids_| indicating the next row to return.
    uint32_t next_row_id_index_ = 0;

    // The timestamp of the row to index. This is either the timestamp of
    // the slice at |next_row_id_index_| or the timestamp of the next quantized
    // group boundary.
    uint64_t next_timestamp_ = 0;

    // The CPU this state is associated with.
    uint32_t cpu_ = 0;

    // The quantum the output slices should fall within.
    uint64_t quantum_ = 0;

    const TraceStorage* storage_ = nullptr;
  };

  // Transient state for a filter operation on a Cursor.
  class FilterState {
   public:
    FilterState(const TraceStorage* storage,
                const QueryConstraints& query_constraints,
                sqlite3_value** argv);

    // Chooses the next CPU which should be returned according to the sorting
    // citeria specified by |order_by_|.
    void FindCpuWithNextSlice();

    // Returns whether the next CPU to be returned by this filter operation is
    // valid.
    bool IsNextCpuValid() const { return next_cpu_ < per_cpu_state_.size(); }

    // Returns the transient state associated with a single CPU.
    PerCpuState* StateForCpu(uint32_t cpu) { return &per_cpu_state_[cpu]; }

    uint32_t next_cpu() const { return next_cpu_; }
    uint64_t quantum() const { return quantum_; }

   private:
    // Creates a vector of indices into the slices for the given |cpu| sorted
    // by the order by criteria.
    std::vector<uint32_t> CreateSortedIndexVectorForCpu(uint32_t cpu,
                                                        uint64_t min_ts,
                                                        uint64_t max_ts);

    // Compares the next slice of the given |cpu| with the next slice of the
    // |next_cpu_|. Return <0 if |cpu| is ordered before, >0 if ordered after,
    // and 0 if they are equal.
    int CompareCpuToNextCpu(uint32_t cpu);

    // Compares the slice at index |f| in |f_slices| for CPU |f_cpu| with the
    // slice at index |s| in |s_slices| for CPU |s_cpu| on all columns.
    // Returns -1 if the first slice is before the second in the ordering, 1 if
    // the first slice is after the second and 0 if they are equal.
    int CompareSlices(uint32_t f_cpu, size_t f, uint32_t s_cpu, size_t s);

    // Compares the slice at index |f| in |f_slices| for CPU |f_cpu| with the
    // slice at index |s| in |s_slices| for CPU |s_cpu| on the criteria in
    // |order_by|.
    // Returns -1 if the first slice is before the second in the ordering, 1 if
    // the first slice is after the second and 0 if they are equal.
    int CompareSlicesOnColumn(uint32_t f_cpu,
                              size_t f,
                              uint32_t s_cpu,
                              size_t s,
                              const QueryConstraints::OrderBy& order_by);

    // One entry for each cpu which is used in filtering.
    std::array<PerCpuState, base::kMaxCpus> per_cpu_state_;

    // The next CPU which should be returned to the user.
    uint32_t next_cpu_ = 0;

    // The quantum the output slices should fall within.
    uint64_t quantum_ = 0;

    // The sorting criteria for this filter operation.
    std::vector<QueryConstraints::OrderBy> order_by_;

    const TraceStorage* const storage_;
  };

  // Implementation of the SQLite cursor interface.
  class Cursor : public Table::Cursor {
   public:
    Cursor(const TraceStorage* storage);

    // Implementation of Table::Cursor.
    int Filter(const QueryConstraints&, sqlite3_value**) override;
    int Next() override;
    int Eof() override;
    int Column(sqlite3_context*, int N) override;

   private:
    const TraceStorage* const storage_;
    std::unique_ptr<FilterState> filter_state_;
  };

  const TraceStorage* const storage_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SCHED_SLICE_TABLE_H_
