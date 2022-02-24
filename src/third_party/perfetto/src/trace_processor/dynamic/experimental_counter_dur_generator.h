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

#ifndef SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_COUNTER_DUR_GENERATOR_H_
#define SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_COUNTER_DUR_GENERATOR_H_

#include "src/trace_processor/sqlite/db_sqlite_table.h"

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class ExperimentalCounterDurGenerator
    : public DbSqliteTable::DynamicTableGenerator {
 public:
  explicit ExperimentalCounterDurGenerator(const tables::CounterTable& table);
  virtual ~ExperimentalCounterDurGenerator() override;

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  util::Status ValidateConstraints(const QueryConstraints&) override;
  std::unique_ptr<Table> ComputeTable(const std::vector<Constraint>&,
                                      const std::vector<Order>&,
                                      const BitVector& cols_used) override;

  // public + static for testing
  static NullableVector<int64_t> ComputeDurColumn(const Table& table);
  static NullableVector<double> ComputeDeltaColumn(const Table& table);

 private:
  const tables::CounterTable* counter_table_ = nullptr;
  std::unique_ptr<NullableVector<int64_t>> dur_column_;
  std::unique_ptr<NullableVector<double>> delta_column_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_COUNTER_DUR_GENERATOR_H_
