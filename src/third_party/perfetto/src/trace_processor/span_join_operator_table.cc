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

#include "src/trace_processor/span_join_operator_table.h"

#include <sqlite3.h>
#include <string.h>
#include <algorithm>
#include <set>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/base/string_splitter.h"
#include "perfetto/base/string_utils.h"
#include "perfetto/base/string_view.h"
#include "src/trace_processor/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {

namespace {

constexpr char kTsColumnName[] = "ts";
constexpr char kDurColumnName[] = "dur";

bool IsRequiredColumn(const std::string& name) {
  return name == kTsColumnName || name == kDurColumnName;
}

base::Optional<std::string> HasDuplicateColumns(
    const std::vector<Table::Column>& cols) {
  std::set<std::string> names;
  for (const auto& col : cols) {
    if (names.count(col.name()) > 0)
      return col.name();
    names.insert(col.name());
  }
  return base::nullopt;
}

}  // namespace

SpanJoinOperatorTable::SpanJoinOperatorTable(sqlite3* db, const TraceStorage*)
    : db_(db) {}

void SpanJoinOperatorTable::RegisterTable(sqlite3* db,
                                          const TraceStorage* storage) {
  Table::Register<SpanJoinOperatorTable>(db, storage, "span_join",
                                         /* read_write */ false,
                                         /* requires_args */ true);

  Table::Register<SpanJoinOperatorTable>(db, storage, "span_left_join",
                                         /* read_write */ false,
                                         /* requires_args */ true);

  Table::Register<SpanJoinOperatorTable>(db, storage, "span_outer_join",
                                         /* read_write */ false,
                                         /* requires_args */ true);
}

util::Status SpanJoinOperatorTable::Init(int argc,
                                         const char* const* argv,
                                         Schema* schema) {
  // argv[0] - argv[2] are SQLite populated fields which are always present.
  if (argc < 5)
    return util::Status("SPAN_JOIN: expected at least 2 args");

  TableDescriptor t1_desc;
  auto status = TableDescriptor::Parse(
      std::string(reinterpret_cast<const char*>(argv[3])), &t1_desc);
  if (!status.ok())
    return status;

  TableDescriptor t2_desc;
  status = TableDescriptor::Parse(
      std::string(reinterpret_cast<const char*>(argv[4])), &t2_desc);
  if (!status.ok())
    return status;

  // Check that the partition columns match between the two tables.
  if (t1_desc.partition_col == t2_desc.partition_col) {
    partitioning_ = t1_desc.IsPartitioned()
                        ? PartitioningType::kSamePartitioning
                        : PartitioningType::kNoPartitioning;
    if (partitioning_ == PartitioningType::kNoPartitioning && IsOuterJoin()) {
      return util::ErrStatus(
          "SPAN_JOIN: Outer join not supported for no partition tables");
    }
  } else if (t1_desc.IsPartitioned() && t2_desc.IsPartitioned()) {
    return util::ErrStatus(
        "SPAN_JOIN: mismatching partitions between the two tables; "
        "(partition %s in table %s, partition %s in table %s)",
        t1_desc.partition_col.c_str(), t1_desc.name.c_str(),
        t2_desc.partition_col.c_str(), t2_desc.name.c_str());
  } else {
    if (IsOuterJoin()) {
      return util::ErrStatus(
          "SPAN_JOIN: Outer join not supported for mixed partitioned tables");
    }
    partitioning_ = PartitioningType::kMixedPartitioning;
  }

  status = CreateTableDefinition(t1_desc, IsOuterJoin(), &t1_defn_);
  if (!status.ok())
    return status;

  status =
      CreateTableDefinition(t2_desc, IsOuterJoin() || IsLeftJoin(), &t2_defn_);
  if (!status.ok())
    return status;

  std::vector<Table::Column> cols;
  // Ensure the shared columns are consistently ordered and are not
  // present twice in the final schema
  cols.emplace_back(Column::kTimestamp, kTsColumnName, ColumnType::kLong);
  cols.emplace_back(Column::kDuration, kDurColumnName, ColumnType::kLong);
  if (partitioning_ != PartitioningType::kNoPartitioning)
    cols.emplace_back(Column::kPartition, partition_col(), ColumnType::kLong);

  CreateSchemaColsForDefn(t1_defn_, &cols);
  CreateSchemaColsForDefn(t2_defn_, &cols);

  if (auto opt_dupe_col = HasDuplicateColumns(cols)) {
    return util::ErrStatus(
        "SPAN_JOIN: column %s present in both tables %s and %s",
        opt_dupe_col->c_str(), t1_defn_.name().c_str(),
        t2_defn_.name().c_str());
  }
  std::vector<size_t> primary_keys = {Column::kTimestamp};
  if (partitioning_ != PartitioningType::kNoPartitioning)
    primary_keys.push_back(Column::kPartition);
  *schema = Schema(cols, primary_keys);

  return util::OkStatus();
}

void SpanJoinOperatorTable::CreateSchemaColsForDefn(
    const TableDefinition& defn,
    std::vector<Table::Column>* cols) {
  for (size_t i = 0; i < defn.columns().size(); i++) {
    const auto& n = defn.columns()[i].name();
    if (IsRequiredColumn(n) || n == defn.partition_col())
      continue;

    ColumnLocator* locator = &global_index_to_column_locator_[cols->size()];
    locator->defn = &defn;
    locator->col_index = i;

    cols->emplace_back(cols->size(), n, defn.columns()[i].type());
  }
}

std::unique_ptr<Table::Cursor> SpanJoinOperatorTable::CreateCursor() {
  return std::unique_ptr<SpanJoinOperatorTable::Cursor>(new Cursor(this, db_));
}

int SpanJoinOperatorTable::BestIndex(const QueryConstraints&, BestIndexInfo*) {
  // TODO(lalitm): figure out cost estimation.
  return SQLITE_OK;
}

std::vector<std::string>
SpanJoinOperatorTable::ComputeSqlConstraintsForDefinition(
    const TableDefinition& defn,
    const QueryConstraints& qc,
    sqlite3_value** argv) {
  std::vector<std::string> constraints;
  for (size_t i = 0; i < qc.constraints().size(); i++) {
    const auto& cs = qc.constraints()[i];
    auto col_name = GetNameForGlobalColumnIndex(defn, cs.iColumn);
    if (col_name == "")
      continue;

    if (col_name == kTsColumnName || col_name == kDurColumnName) {
      // We don't support constraints on ts or duration in the child tables.
      PERFETTO_DFATAL("ts or duration constraints on child tables");
      continue;
    }
    auto op = sqlite_utils::OpToString(cs.op);
    auto value = sqlite_utils::SqliteValueAsString(argv[i]);

    constraints.emplace_back("`" + col_name + "`" + op + value);
  }
  return constraints;
}

util::Status SpanJoinOperatorTable::CreateTableDefinition(
    const TableDescriptor& desc,
    bool emit_shadow_slices,
    SpanJoinOperatorTable::TableDefinition* defn) {
  if (desc.partition_col == kTsColumnName ||
      desc.partition_col == kDurColumnName) {
    return util::ErrStatus(
        "SPAN_JOIN: partition column cannot be any of {ts, dur} for table %s",
        desc.name.c_str());
  }

  auto cols = sqlite_utils::GetColumnsForTable(db_, desc.name);

  uint32_t required_columns_found = 0;
  uint32_t ts_idx = std::numeric_limits<uint32_t>::max();
  uint32_t dur_idx = std::numeric_limits<uint32_t>::max();
  uint32_t partition_idx = std::numeric_limits<uint32_t>::max();
  for (uint32_t i = 0; i < cols.size(); i++) {
    auto col = cols[i];
    if (IsRequiredColumn(col.name())) {
      ++required_columns_found;
      if (col.type() != Table::ColumnType::kLong &&
          col.type() != Table::ColumnType::kUnknown) {
        return util::ErrStatus(
            "SPAN_JOIN: Invalid type for column %s in table %s",
            col.name().c_str(), desc.name.c_str());
      }
    }

    if (col.name() == kTsColumnName) {
      ts_idx = i;
    } else if (col.name() == kDurColumnName) {
      dur_idx = i;
    } else if (col.name() == desc.partition_col) {
      partition_idx = i;
    }
  }
  if (required_columns_found != 2) {
    return util::ErrStatus(
        "SPAN_JOIN: Missing one of columns {ts, dur} in table %s",
        desc.name.c_str());
  } else if (desc.IsPartitioned() && partition_idx >= cols.size()) {
    return util::ErrStatus("SPAN_JOIN: Missing partition column %s in table %s",
                           desc.partition_col.c_str(), desc.name.c_str());
  }

  PERFETTO_DCHECK(ts_idx < cols.size());
  PERFETTO_DCHECK(dur_idx < cols.size());

  *defn = TableDefinition(desc.name, desc.partition_col, std::move(cols),
                          emit_shadow_slices, ts_idx, dur_idx, partition_idx);
  return util::OkStatus();
}

std::string SpanJoinOperatorTable::GetNameForGlobalColumnIndex(
    const TableDefinition& defn,
    int global_column) {
  size_t col_idx = static_cast<size_t>(global_column);
  if (col_idx == Column::kTimestamp)
    return kTsColumnName;
  else if (col_idx == Column::kDuration)
    return kDurColumnName;
  else if (col_idx == Column::kPartition &&
           partitioning_ != PartitioningType::kNoPartitioning)
    return defn.partition_col().c_str();

  const auto& locator = global_index_to_column_locator_[col_idx];
  if (locator.defn != &defn)
    return "";
  return defn.columns()[locator.col_index].name().c_str();
}

SpanJoinOperatorTable::Cursor::Cursor(SpanJoinOperatorTable* table, sqlite3* db)
    : Table::Cursor(table),
      t1_(table, &table->t1_defn_, db),
      t2_(table, &table->t2_defn_, db),
      table_(table) {}

int SpanJoinOperatorTable::Cursor::Filter(const QueryConstraints& qc,
                                          sqlite3_value** argv) {
  int err = t1_.Initialize(qc, argv);
  if (err != SQLITE_OK)
    return err;

  err = t2_.Initialize(qc, argv);
  if (err != SQLITE_OK)
    return err;

  // Step the partitioned table to allow for us to look into it below.
  Query* step_now = t1_.IsPartitioned() ? &t1_ : &t2_;
  next_stepped_ = step_now == &t1_ ? &t2_ : &t1_;

  auto res = step_now->Step();
  if (PERFETTO_UNLIKELY(res.is_err()))
    return res.err_code;

  // Forward the unpartitioned table to reflect the partition of the partitoined
  // table.
  if (table_->partitioning_ == PartitioningType::kMixedPartitioning) {
    PERFETTO_DCHECK(step_now->IsPartitioned());

    // If we emit shadow slices, we need to step because the first slice will
    // be a full partition shadow slice that we need to skip.
    if (step_now->definition()->emit_shadow_slices()) {
      PERFETTO_DCHECK(step_now->IsFullPartitionShadowSlice());
      res = step_now->StepToNextPartition();
      if (PERFETTO_UNLIKELY(res.is_err()))
        return res.err_code;
    }

    res = next_stepped_->StepToPartition(step_now->partition());
    if (PERFETTO_UNLIKELY(res.is_err()))
      return res.err_code;
  }

  // Otherwise, find an overlapping span.
  return Next();
}

bool SpanJoinOperatorTable::Cursor::IsOverlappingSpan() {
  if (!t1_.IsRealSlice() && !t2_.IsRealSlice()) {
    return false;
  } else if (t1_.partition() != t2_.partition()) {
    return false;
  } else if (t1_.ts_end() <= t2_.ts_start() || t2_.ts_end() <= t1_.ts_start()) {
    return false;
  }
  return true;
}

int SpanJoinOperatorTable::Cursor::Next() {
  // TODO: Propagate error msg to the table.
  auto res = next_stepped_->Step();
  if (res.is_err())
    return res.err_code;

  while (true) {
    if (t1_.Eof() || t2_.Eof()) {
      if (table_->partitioning_ != PartitioningType::kMixedPartitioning)
        return SQLITE_OK;

      auto* partitioned = t1_.IsPartitioned() ? &t1_ : &t2_;
      auto* unpartitioned = t1_.IsPartitioned() ? &t2_ : &t1_;
      if (partitioned->Eof())
        return SQLITE_OK;

      res = partitioned->StepToNextPartition();
      if (PERFETTO_UNLIKELY(res.is_err()))
        return res.err_code;
      else if (PERFETTO_UNLIKELY(res.is_eof()))
        continue;

      res = unpartitioned->StepToPartition(partitioned->partition());
      if (PERFETTO_UNLIKELY(res.is_err()))
        return res.err_code;
      else if (PERFETTO_UNLIKELY(res.is_eof()))
        continue;
    }

    int64_t partition = std::max(t1_.partition(), t2_.partition());
    res = t1_.StepToPartition(partition);
    if (PERFETTO_UNLIKELY(res.is_err()))
      return res.err_code;
    else if (PERFETTO_UNLIKELY(res.is_eof()))
      continue;

    res = t2_.StepToPartition(t1_.partition());
    if (PERFETTO_UNLIKELY(res.is_err()))
      return res.err_code;
    else if (PERFETTO_UNLIKELY(res.is_eof()))
      continue;

    if (t1_.partition() != t2_.partition())
      continue;

    auto ts = std::max(t1_.ts_start(), t2_.ts_start());
    res = t1_.StepUntil(ts);
    if (PERFETTO_UNLIKELY(res.is_err()))
      return res.err_code;
    else if (PERFETTO_UNLIKELY(res.is_eof()))
      continue;

    res = t2_.StepUntil(t1_.ts_start());
    if (PERFETTO_UNLIKELY(res.is_err()))
      return res.err_code;
    else if (PERFETTO_UNLIKELY(res.is_eof()))
      continue;

    // If we're in the case where we have shadow slices on both tables, try
    // and forward the earliest table and see what happens. IsOverlappingSpan()
    // will double check that we have at least one non-real slice now.
    // Note: if we don't do this, we end up in an infinite loop because all
    // the code above will not change anything because these shadow slices will
    // be overlapping.
    if (!t1_.IsRealSlice() && !t2_.IsRealSlice()) {
      PERFETTO_DCHECK(t1_.partition() == t2_.partition());

      // If the table is not partitioned, partition() will return the partition
      // the table was set to have by StepToPartition().
      auto t1_partition =
          t1_.IsPartitioned() ? t1_.CursorPartition() : t1_.partition();
      auto t2_partition =
          t2_.IsPartitioned() ? t2_.CursorPartition() : t2_.partition();

      Query* stepped;
      if (t1_partition == t2_partition) {
        stepped = t1_.ts_end() <= t2_.ts_end() ? &t1_ : &t2_;
      } else {
        stepped = t1_partition <= t2_partition ? &t1_ : &t2_;
      }

      res = stepped->Step();
      if (PERFETTO_UNLIKELY(res.is_err()))
        return res.err_code;
      else if (PERFETTO_UNLIKELY(res.is_eof()))
        continue;
    }

    if (IsOverlappingSpan())
      break;
  }
  next_stepped_ = t1_.ts_end() <= t2_.ts_end() ? &t1_ : &t2_;

  return SQLITE_OK;
}

int SpanJoinOperatorTable::Cursor::Eof() {
  return t1_.Eof() || t2_.Eof();
}

int SpanJoinOperatorTable::Cursor::Column(sqlite3_context* context, int N) {
  PERFETTO_DCHECK(!t1_.Eof());
  PERFETTO_DCHECK(!t2_.Eof());

  if (N == Column::kTimestamp) {
    auto max_ts = std::max(t1_.ts_start(), t2_.ts_start());
    sqlite3_result_int64(context, static_cast<sqlite3_int64>(max_ts));
  } else if (N == Column::kDuration) {
    auto max_start = std::max(t1_.ts_start(), t2_.ts_start());
    auto min_end = std::min(t1_.ts_end(), t2_.ts_end());
    PERFETTO_DCHECK(min_end > max_start);
    auto dur = min_end - max_start;
    sqlite3_result_int64(context, static_cast<sqlite3_int64>(dur));
  } else if (N == Column::kPartition &&
             table_->partitioning_ != PartitioningType::kNoPartitioning) {
    PERFETTO_DCHECK(table_->partitioning_ ==
                        PartitioningType::kMixedPartitioning ||
                    t1_.partition() == t2_.partition());
    auto partition = t1_.IsPartitioned() ? t1_.partition() : t2_.partition();
    sqlite3_result_int64(context, static_cast<sqlite3_int64>(partition));
  } else {
    size_t index = static_cast<size_t>(N);
    const auto& locator = table_->global_index_to_column_locator_[index];
    if (locator.defn == t1_.definition())
      t1_.ReportSqliteResult(context, locator.col_index);
    else
      t2_.ReportSqliteResult(context, locator.col_index);
  }
  return SQLITE_OK;
}

SpanJoinOperatorTable::Query::Query(SpanJoinOperatorTable* table,
                                    const TableDefinition* definition,
                                    sqlite3* db)
    : defn_(definition), db_(db), table_(table) {
  PERFETTO_DCHECK(!defn_->IsPartitioned() ||
                  defn_->partition_idx() < defn_->columns().size());
}

SpanJoinOperatorTable::Query::~Query() = default;

int SpanJoinOperatorTable::Query::Initialize(const QueryConstraints& qc,
                                             sqlite3_value** argv) {
  *this = Query(table_, definition(), db_);
  sql_query_ = CreateSqlQuery(
      table_->ComputeSqlConstraintsForDefinition(*defn_, qc, argv));
  return PrepareRawStmt();
}

SpanJoinOperatorTable::Query::StepRet SpanJoinOperatorTable::Query::Step() {
  PERFETTO_DCHECK(!Eof());
  sqlite3_stmt* stmt = stmt_.get();

  // In this loop, we will try and find the slice to from the cursor.
  // Terminology: "Shadow slices" are slices which fill in the gaps between real
  // slices from the underlying cursor in each partition (if any).
  // For queries which don't need "shadow slices", we simply return non-zero
  // duration slices from the underlying cursor.
  do {
    if (mode_ == Mode::kShadowSlice) {
      PERFETTO_DCHECK(defn_->emit_shadow_slices());

      // If we're out of slices in the cursor, this shadow slice will be the
      // final slice.
      if (cursor_eof_) {
        mode_ = Mode::kRealSlice;
        return StepRet(StepRet::Code::kEof);
      }

      // Look ahead to see if the cursor changes partition (if the cursor is
      // partitioned). If so, then we need to fill the gap between the ts == 0
      // and the start of that slice. Otherwise after this slice, we have the
      // real slice from the cursor.
      if (!defn_->IsPartitioned() || partition_ == CursorPartition()) {
        mode_ = Mode::kRealSlice;
        ts_start_ = CursorTs();
        ts_end_ = ts_start_ + CursorDur();
      } else if (IsFullPartitionShadowSlice()) {
        mode_ = Mode::kShadowSlice;
        ts_start_ = 0;
        ts_end_ = CursorTs();
        partition_ = CursorPartition();
      } else {
        mode_ = Mode::kShadowSlice;
        ts_start_ = 0;
        ts_end_ = std::numeric_limits<int64_t>::max();
      }
      continue;
    }

    int res;
    if (defn_->IsPartitioned()) {
      auto partition_idx = static_cast<int>(defn_->partition_idx());
      // Fastforward through any rows with null partition keys.
      int row_type;
      do {
        res = sqlite3_step(stmt);
        row_type = sqlite3_column_type(stmt, partition_idx);
      } while (res == SQLITE_ROW && row_type == SQLITE_NULL);
    } else {
      res = sqlite3_step(stmt);
    }

    if (res == SQLITE_ROW) {
      // After every row, there will be a shadow slice so emit that if we need
      // to do so. Otherwise, just emit the underlying slice.
      if (defn_->emit_shadow_slices()) {
        mode_ = Mode::kShadowSlice;
        ts_start_ = ts_end_;
        ts_end_ = !defn_->IsPartitioned() || partition_ == CursorPartition()
                      ? CursorTs()
                      : std::numeric_limits<int64_t>::max();
      } else {
        mode_ = Mode::kRealSlice;
        ts_start_ = CursorTs();
        ts_end_ = ts_start_ + CursorDur();
        if (defn_->IsPartitioned())
          partition_ = CursorPartition();
      }
    } else if (res == SQLITE_DONE) {
      cursor_eof_ = true;
      if (!defn_->emit_shadow_slices())
        return StepRet(StepRet::Code::kEof);

      // Close off the remainder of this partition with a shadow slice.
      mode_ = Mode::kShadowSlice;
      ts_start_ = ts_end_;
      ts_end_ = std::numeric_limits<int64_t>::max();
    } else {
      return StepRet(StepRet::Code::kError, res);
    }
  } while (ts_start_ == ts_end_);

  return StepRet(StepRet::Code::kRow);
}

SpanJoinOperatorTable::Query::StepRet
SpanJoinOperatorTable::Query::StepToNextPartition() {
  PERFETTO_DCHECK(defn_->IsPartitioned());
  PERFETTO_DCHECK(!Eof());

  auto current_partition = partition_;
  while (partition_ <= current_partition) {
    auto res = Step();
    if (!res.is_row())
      return res;
  }
  return StepRet(StepRet::Code::kRow);
}

SpanJoinOperatorTable::Query::StepRet
SpanJoinOperatorTable::Query::StepToPartition(int64_t target_partition) {
  PERFETTO_DCHECK(partition_ <= target_partition);
  if (defn_->IsPartitioned()) {
    while (partition_ < target_partition) {
      if (IsFullPartitionShadowSlice() &&
          (cursor_eof_ || target_partition < CursorPartition())) {
        partition_ = target_partition;
        return StepRet(StepRet::Code::kRow);
      }

      auto res = Step();
      if (!res.is_row())
        return res;
    }
  } else if (/* !defn_->IsPartitioned() && */ partition_ < target_partition) {
    int res = PrepareRawStmt();
    if (res != SQLITE_OK)
      return StepRet(StepRet::Code::kError, res);
    partition_ = target_partition;
  }
  return StepRet(StepRet::Code::kRow);
}

SpanJoinOperatorTable::Query::StepRet SpanJoinOperatorTable::Query::StepUntil(
    int64_t timestamp) {
  PERFETTO_DCHECK(!Eof());
  auto partition = partition_;
  while (partition_ == partition && ts_end_ <= timestamp) {
    auto res = Step();
    if (!res.is_row())
      return res;
  }
  return StepRet(StepRet::Code::kRow);
}

std::string SpanJoinOperatorTable::Query::CreateSqlQuery(
    const std::vector<std::string>& cs) const {
  std::vector<std::string> col_names;
  for (const Table::Column& c : defn_->columns()) {
    col_names.push_back("`" + c.name() + "`");
  }

  std::string sql = "SELECT " + base::Join(col_names, ", ");
  sql += " FROM " + defn_->name();
  if (!cs.empty()) {
    sql += " WHERE " + base::Join(cs, " AND ");
  }
  sql += " ORDER BY ";
  sql += defn_->IsPartitioned()
             ? base::Join({"`" + defn_->partition_col() + "`", "ts"}, ", ")
             : "ts";
  sql += ";";
  PERFETTO_DLOG("%s", sql.c_str());
  return sql;
}

int SpanJoinOperatorTable::Query::PrepareRawStmt() {
  sqlite3_stmt* stmt = nullptr;
  int err =
      sqlite3_prepare_v2(db_, sql_query_.c_str(),
                         static_cast<int>(sql_query_.size()), &stmt, nullptr);
  stmt_.reset(stmt);

  ts_start_ = 0;
  ts_end_ = 0;
  partition_ = std::numeric_limits<int64_t>::lowest();
  cursor_eof_ = false;
  mode_ = Mode::kRealSlice;

  return err;
}

void SpanJoinOperatorTable::Query::ReportSqliteResult(sqlite3_context* context,
                                                      size_t index) {
  if (mode_ != Mode::kRealSlice) {
    sqlite3_result_null(context);
    return;
  }

  sqlite3_stmt* stmt = stmt_.get();
  int idx = static_cast<int>(index);
  switch (sqlite3_column_type(stmt, idx)) {
    case SQLITE_INTEGER:
      sqlite3_result_int64(context, sqlite3_column_int64(stmt, idx));
      break;
    case SQLITE_FLOAT:
      sqlite3_result_double(context, sqlite3_column_double(stmt, idx));
      break;
    case SQLITE_TEXT: {
      // TODO(lalitm): note for future optimizations: if we knew the addresses
      // of the string intern pool, we could check if the string returned here
      // comes from the pool, and pass it as non-transient.
      const auto kSqliteTransient =
          reinterpret_cast<sqlite3_destructor_type>(-1);
      auto ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
      sqlite3_result_text(context, ptr, -1, kSqliteTransient);
      break;
    }
  }
}

SpanJoinOperatorTable::TableDefinition::TableDefinition(
    std::string name,
    std::string partition_col,
    std::vector<Table::Column> cols,
    bool emit_shadow_slices,
    uint32_t ts_idx,
    uint32_t dur_idx,
    uint32_t partition_idx)
    : name_(std::move(name)),
      partition_col_(std::move(partition_col)),
      cols_(std::move(cols)),
      emit_shadow_slices_(emit_shadow_slices),
      ts_idx_(ts_idx),
      dur_idx_(dur_idx),
      partition_idx_(partition_idx) {}

util::Status SpanJoinOperatorTable::TableDescriptor::Parse(
    const std::string& raw_descriptor,
    SpanJoinOperatorTable::TableDescriptor* descriptor) {
  // Descriptors have one of the following forms:
  // table_name [PARTITIONED column_name]

  // Find the table name.
  base::StringSplitter splitter(raw_descriptor, ' ');
  if (!splitter.Next())
    return util::ErrStatus("SPAN_JOIN: Missing table name");

  descriptor->name = splitter.cur_token();
  if (!splitter.Next())
    return util::OkStatus();

  if (strcasecmp(splitter.cur_token(), "PARTITIONED") != 0)
    return util::ErrStatus("SPAN_JOIN: Invalid token");

  if (!splitter.Next())
    return util::ErrStatus("SPAN_JOIN: Missing partitioning column");

  descriptor->partition_col = splitter.cur_token();
  return util::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto
