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

#include "src/trace_processor/stats_table.h"

#include "src/trace_processor/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {

StatsTable::StatsTable(sqlite3*, const TraceStorage* storage)
    : storage_(storage) {}

void StatsTable::RegisterTable(sqlite3* db, const TraceStorage* storage) {
  Table::Register<StatsTable>(db, storage, "stats");
}

base::Optional<Table::Schema> StatsTable::Init(int, const char* const*) {
  return Schema(
      {
          Table::Column(Column::kName, "name", ColumnType::kString),
          // Calling a column "index" causes sqlite to silently fail, hence idx.
          Table::Column(Column::kIndex, "idx", ColumnType::kUint),
          Table::Column(Column::kSeverity, "severity", ColumnType::kString),
          Table::Column(Column::kSource, "source", ColumnType::kString),
          Table::Column(Column::kValue, "value", ColumnType::kLong),
      },
      {Column::kName});
}

std::unique_ptr<Table::Cursor> StatsTable::CreateCursor(const QueryConstraints&,
                                                        sqlite3_value**) {
  return std::unique_ptr<Table::Cursor>(new Cursor(storage_));
}

int StatsTable::BestIndex(const QueryConstraints&, BestIndexInfo*) {
  return SQLITE_OK;
}

StatsTable::Cursor::Cursor(const TraceStorage* storage) : storage_(storage) {}

int StatsTable::Cursor::Column(sqlite3_context* ctx, int N) {
  const auto kSqliteStatic = sqlite_utils::kSqliteStatic;
  switch (N) {
    case Column::kName:
      sqlite3_result_text(ctx, stats::kNames[key_], -1, kSqliteStatic);
      break;
    case Column::kIndex:
      if (stats::kTypes[key_] == stats::kIndexed) {
        sqlite3_result_int(ctx, index_->first);
      } else {
        sqlite3_result_null(ctx);
      }
      break;
    case Column::kSeverity:
      switch (stats::kSeverities[key_]) {
        case stats::kInfo:
          sqlite3_result_text(ctx, "info", -1, kSqliteStatic);
          break;
        case stats::kError:
          sqlite3_result_text(ctx, "error", -1, kSqliteStatic);
          break;
      }
      break;
    case Column::kSource:
      switch (stats::kSources[key_]) {
        case stats::kTrace:
          sqlite3_result_text(ctx, "trace", -1, kSqliteStatic);
          break;
        case stats::kAnalysis:
          sqlite3_result_text(ctx, "analysis", -1, kSqliteStatic);
          break;
      }
      break;
    case Column::kValue:
      if (stats::kTypes[key_] == stats::kIndexed) {
        sqlite3_result_int64(ctx, index_->second);
      } else {
        sqlite3_result_int64(ctx, storage_->stats()[key_].value);
      }
      break;
    default:
      PERFETTO_FATAL("Unknown column %d", N);
      break;
  }
  return SQLITE_OK;
}

int StatsTable::Cursor::Next() {
  static_assert(stats::kTypes[0] == stats::kSingle,
                "the first stats entry cannot be indexed");
  const auto* cur_entry = &storage_->stats()[key_];
  if (stats::kTypes[key_] == stats::kIndexed) {
    if (++index_ != cur_entry->indexed_values.end()) {
      return SQLITE_OK;
    }
  }
  while (++key_ < stats::kNumKeys) {
    cur_entry = &storage_->stats()[key_];
    index_ = cur_entry->indexed_values.begin();
    if (stats::kTypes[key_] == stats::kSingle ||
        !cur_entry->indexed_values.empty()) {
      break;
    }
  }
  return SQLITE_OK;
}

int StatsTable::Cursor::Eof() {
  return key_ >= stats::kNumKeys;
}

}  // namespace trace_processor
}  // namespace perfetto
