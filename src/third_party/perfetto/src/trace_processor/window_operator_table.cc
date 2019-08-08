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

#include "src/trace_processor/window_operator_table.h"

#include "src/trace_processor/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {

namespace {
using namespace sqlite_utils;
}  // namespace

WindowOperatorTable::WindowOperatorTable(sqlite3*, const TraceStorage*) {}

void WindowOperatorTable::RegisterTable(sqlite3* db,
                                        const TraceStorage* storage) {
  Table::Register<WindowOperatorTable>(db, storage, "window", true);
}

util::Status WindowOperatorTable::Init(int,
                                       const char* const*,
                                       Schema* schema) {
  const bool kHidden = true;
  *schema = Schema(
      {
          // These are the operator columns:
          Table::Column(Column::kRowId, "rowid", ColumnType::kLong, kHidden),
          Table::Column(Column::kQuantum, "quantum", ColumnType::kLong,
                        kHidden),
          Table::Column(Column::kWindowStart, "window_start", ColumnType::kLong,
                        kHidden),
          Table::Column(Column::kWindowDur, "window_dur", ColumnType::kLong,
                        kHidden),
          // These are the ouput columns:
          Table::Column(Column::kTs, "ts", ColumnType::kLong),
          Table::Column(Column::kDuration, "dur", ColumnType::kLong),
          Table::Column(Column::kQuantumTs, "quantum_ts", ColumnType::kLong),
      },
      {Column::kRowId});
  return util::OkStatus();
}

std::unique_ptr<Table::Cursor> WindowOperatorTable::CreateCursor() {
  return std::unique_ptr<Table::Cursor>(new Cursor(this));
}

int WindowOperatorTable::BestIndex(const QueryConstraints& qc,
                                   BestIndexInfo* info) {
  // Remove ordering on timestamp if it is the only ordering as we are already
  // sorted on TS. This makes span joining significantly faster.
  if (qc.order_by().size() == 1 && qc.order_by()[0].iColumn == Column::kTs &&
      !qc.order_by()[0].desc) {
    info->order_by_consumed = true;
  }
  return SQLITE_OK;
}

int WindowOperatorTable::Update(int argc,
                                sqlite3_value** argv,
                                sqlite3_int64*) {
  // We only support updates to ts and dur. Disallow deletes (argc == 1) and
  // inserts (argv[0] == null).
  if (argc < 2 || sqlite3_value_type(argv[0]) == SQLITE_NULL)
    return SQLITE_READONLY;

  int64_t new_quantum = sqlite3_value_int64(argv[3]);
  int64_t new_start = sqlite3_value_int64(argv[4]);
  int64_t new_dur = sqlite3_value_int64(argv[5]);
  if (new_dur == 0) {
    auto* err = sqlite3_mprintf("Cannot set duration of window table to zero.");
    SetErrorMessage(err);
    return SQLITE_ERROR;
  }

  quantum_ = new_quantum;
  window_start_ = new_start;
  window_dur_ = new_dur;

  return SQLITE_OK;
}

WindowOperatorTable::Cursor::Cursor(WindowOperatorTable* table)
    : Table::Cursor(table), table_(table) {}

int WindowOperatorTable::Cursor::Filter(const QueryConstraints& qc,
                                        sqlite3_value** argv) {
  *this = Cursor(table_);
  window_start_ = table_->window_start_;
  window_end_ = table_->window_start_ + table_->window_dur_;
  step_size_ = table_->quantum_ == 0 ? table_->window_dur_ : table_->quantum_;

  current_ts_ = window_start_;

  // Set return first if there is a equals constraint on the row id asking to
  // return the first row.
  bool return_first = qc.constraints().size() == 1 &&
                      qc.constraints()[0].iColumn == Column::kRowId &&
                      IsOpEq(qc.constraints()[0].op) &&
                      sqlite3_value_int(argv[0]) == 0;
  if (return_first) {
    filter_type_ = FilterType::kReturnFirst;
  } else {
    filter_type_ = FilterType::kReturnAll;
  }
  return SQLITE_OK;
}

int WindowOperatorTable::Cursor::Column(sqlite3_context* context, int N) {
  switch (N) {
    case Column::kQuantum: {
      sqlite3_result_int64(context,
                           static_cast<sqlite_int64>(table_->quantum_));
      break;
    }
    case Column::kWindowStart: {
      sqlite3_result_int64(context,
                           static_cast<sqlite_int64>(table_->window_start_));
      break;
    }
    case Column::kWindowDur: {
      sqlite3_result_int(context, static_cast<int>(table_->window_dur_));
      break;
    }
    case Column::kTs: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(current_ts_));
      break;
    }
    case Column::kDuration: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(step_size_));
      break;
    }
    case Column::kQuantumTs: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(quantum_ts_));
      break;
    }
    case Column::kRowId: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(row_id_));
      break;
    }
    default: {
      PERFETTO_FATAL("Unknown column %d", N);
      break;
    }
  }
  return SQLITE_OK;
}

int WindowOperatorTable::Cursor::Next() {
  switch (filter_type_) {
    case FilterType::kReturnFirst:
      current_ts_ = window_end_;
      break;
    case FilterType::kReturnAll:
      current_ts_ += step_size_;
      quantum_ts_++;
      break;
  }
  row_id_++;
  return SQLITE_OK;
}

int WindowOperatorTable::Cursor::Eof() {
  return current_ts_ >= window_end_;
}

}  // namespace trace_processor
}  // namespace perfetto
