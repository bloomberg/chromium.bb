/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_DYNAMIC_DESCRIBE_SLICE_GENERATOR_H_
#define SRC_TRACE_PROCESSOR_DYNAMIC_DESCRIBE_SLICE_GENERATOR_H_

#include "src/trace_processor/sqlite/db_sqlite_table.h"

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// Dynamic table for implementing the describe_slice table.
// See /docs/analysis.md for details about the functionality and usage of this
// table.
class DescribeSliceGenerator : public DbSqliteTable::DynamicTableGenerator {
 public:
  struct InputValues {
    uint32_t slice_id_value;
  };

  explicit DescribeSliceGenerator(TraceProcessorContext* context);
  ~DescribeSliceGenerator() override;

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  util::Status ValidateConstraints(const QueryConstraints&) override;
  std::unique_ptr<Table> ComputeTable(const std::vector<Constraint>& cs,
                                      const std::vector<Order>& ob,
                                      const BitVector& cols_used) override;

 private:
  TraceProcessorContext* context_ = nullptr;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DYNAMIC_DESCRIBE_SLICE_GENERATOR_H_
