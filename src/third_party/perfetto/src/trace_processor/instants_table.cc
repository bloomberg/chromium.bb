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

#include "src/trace_processor/instants_table.h"

#include "src/trace_processor/storage_cursor.h"
#include "src/trace_processor/table_utils.h"

namespace perfetto {
namespace trace_processor {

InstantsTable::InstantsTable(sqlite3*, const TraceStorage* storage)
    : storage_(storage) {
  ref_types_.resize(RefType::kRefMax);
  ref_types_[RefType::kRefNoRef] = "";
  ref_types_[RefType::kRefUtid] = "utid";
  ref_types_[RefType::kRefCpuId] = "cpu";
  ref_types_[RefType::kRefIrq] = "irq";
  ref_types_[RefType::kRefSoftIrq] = "softirq";
  ref_types_[RefType::kRefUpid] = "upid";
};

void InstantsTable::RegisterTable(sqlite3* db, const TraceStorage* storage) {
  Table::Register<InstantsTable>(db, storage, "instants");
}

Table::Schema InstantsTable::CreateSchema(int, const char* const*) {
  const auto& instants = storage_->instants();
  std::unique_ptr<StorageSchema::Column> cols[] = {
      StorageSchema::NumericColumnPtr("ts", &instants.timestamps(),
                                      false /* hidden */, true /* ordered */),
      StorageSchema::StringColumnPtr("name", &instants.name_ids(),
                                     &storage_->string_pool()),
      StorageSchema::NumericColumnPtr("value", &instants.values()),
      StorageSchema::NumericColumnPtr("ref", &instants.refs()),
      StorageSchema::StringColumnPtr("ref_type", &instants.types(),
                                     &ref_types_)};
  schema_ = StorageSchema({
      std::make_move_iterator(std::begin(cols)),
      std::make_move_iterator(std::end(cols)),
  });
  return schema_.ToTableSchema({"name", "ts", "ref"});
}

std::unique_ptr<Table::Cursor> InstantsTable::CreateCursor(
    const QueryConstraints& qc,
    sqlite3_value** argv) {
  uint32_t count = static_cast<uint32_t>(storage_->instants().instant_count());
  auto it = table_utils::CreateBestRowIteratorForGenericSchema(schema_, count,
                                                               qc, argv);
  return std::unique_ptr<Table::Cursor>(
      new StorageCursor(std::move(it), schema_.ToColumnReporters()));
}

int InstantsTable::BestIndex(const QueryConstraints& qc, BestIndexInfo* info) {
  info->estimated_cost =
      static_cast<uint32_t>(storage_->counters().counter_count());

  // Only the string columns are handled by SQLite
  info->order_by_consumed = true;
  size_t name_index = schema_.ColumnIndexFromName("name");
  size_t ref_type_index = schema_.ColumnIndexFromName("ref_type");
  for (size_t i = 0; i < qc.constraints().size(); i++) {
    info->omit[i] =
        qc.constraints()[i].iColumn != static_cast<int>(name_index) &&
        qc.constraints()[i].iColumn != static_cast<int>(ref_type_index);
  }

  return SQLITE_OK;
}
}  // namespace trace_processor
}  // namespace perfetto
