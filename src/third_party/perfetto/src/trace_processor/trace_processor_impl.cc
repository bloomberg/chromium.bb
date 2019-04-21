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

#include "src/trace_processor/trace_processor_impl.h"

#include <inttypes.h>
#include <algorithm>
#include <functional>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "src/trace_processor/android_logs_table.h"
#include "src/trace_processor/args_table.h"
#include "src/trace_processor/args_tracker.h"
#include "src/trace_processor/clock_tracker.h"
#include "src/trace_processor/counter_definitions_table.h"
#include "src/trace_processor/counter_values_table.h"
#include "src/trace_processor/event_tracker.h"
#include "src/trace_processor/instants_table.h"
#include "src/trace_processor/process_table.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/proto_trace_parser.h"
#include "src/trace_processor/proto_trace_tokenizer.h"
#include "src/trace_processor/raw_table.h"
#include "src/trace_processor/sched_slice_table.h"
#include "src/trace_processor/slice_table.h"
#include "src/trace_processor/slice_tracker.h"
#include "src/trace_processor/span_join_operator_table.h"
#include "src/trace_processor/sql_stats_table.h"
#include "src/trace_processor/sqlite3_str_split.h"
#include "src/trace_processor/stats_table.h"
#include "src/trace_processor/string_table.h"
#include "src/trace_processor/syscall_tracker.h"
#include "src/trace_processor/table.h"
#include "src/trace_processor/thread_table.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_sorter.h"
#include "src/trace_processor/window_operator_table.h"

// JSON parsing is only supported in the standalone build.
#if PERFETTO_BUILDFLAG(PERFETTO_STANDALONE_BUILD)
#include "src/trace_processor/json_trace_parser.h"
#include "src/trace_processor/json_trace_tokenizer.h"
#endif

// In Android tree builds, we don't have the percentile module.
// Just don't include it.
#if !PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
// defined in sqlite_src/ext/misc/percentile.c
extern "C" int sqlite3_percentile_init(sqlite3* db,
                                       char** error,
                                       const sqlite3_api_routines* api);
#endif

namespace perfetto {
namespace trace_processor {
namespace {

void InitializeSqlite(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db, "PRAGMA temp_store=2", 0, 0, &error);
  if (error) {
    PERFETTO_FATAL("Error setting pragma temp_store: %s", error);
  }
  sqlite3_str_split_init(db);
// In Android tree builds, we don't have the percentile module.
// Just don't include it.
#if !PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  sqlite3_percentile_init(db, &error, nullptr);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
#endif
}

void BuildBoundsTable(sqlite3* db, std::pair<int64_t, int64_t> bounds) {
  char* error = nullptr;
  sqlite3_exec(db, "DELETE FROM trace_bounds", nullptr, nullptr, &error);
  if (error) {
    PERFETTO_ELOG("Error deleting from bounds table: %s", error);
    sqlite3_free(error);
    return;
  }

  char* insert_sql = sqlite3_mprintf("INSERT INTO trace_bounds VALUES(%" PRId64
                                     ", %" PRId64 ")",
                                     bounds.first, bounds.second);

  sqlite3_exec(db, insert_sql, 0, 0, &error);
  sqlite3_free(insert_sql);
  if (error) {
    PERFETTO_ELOG("Error inserting bounds table: %s", error);
    sqlite3_free(error);
  }
}

void CreateBuiltinTables(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db, "CREATE TABLE perfetto_tables(name STRING)", 0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
  sqlite3_exec(db,
               "CREATE TABLE trace_bounds(start_ts BIG INT, end_ts BIG INT)", 0,
               0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  // Initialize the bounds table with some data so even before parsing any data,
  // we still have a valid table.
  BuildBoundsTable(db, std::make_pair(0, 0));
}

void CreateBuiltinViews(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db,
               "CREATE VIEW counters AS "
               "SELECT * FROM counter_values "
               "INNER JOIN counter_definitions USING(counter_id);",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
}

bool IsPrefix(const std::string& a, const std::string& b) {
  return a.size() <= b.size() && b.substr(0, a.size()) == a;
}

std::string RemoveWhitespace(const std::string& input) {
  std::string str(input);
  str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
  return str;
}

}  // namespace

TraceType GuessTraceType(const uint8_t* data, size_t size) {
  if (size == 0)
    return kUnknownTraceType;
  std::string start(reinterpret_cast<const char*>(data),
                    std::min<size_t>(size, 20));
  std::string start_minus_white_space = RemoveWhitespace(start);
  if (IsPrefix("{\"traceEvents\":[", start_minus_white_space))
    return kJsonTraceType;
  if (IsPrefix("[{", start_minus_white_space))
    return kJsonTraceType;
  return kProtoTraceType;
}

TraceProcessorImpl::TraceProcessorImpl(const Config& cfg) : cfg_(cfg) {
  sqlite3* db = nullptr;
  PERFETTO_CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
  InitializeSqlite(db);
  CreateBuiltinTables(db);
  CreateBuiltinViews(db);
  db_.reset(std::move(db));

  context_.storage.reset(new TraceStorage());
  context_.args_tracker.reset(new ArgsTracker(&context_));
  context_.slice_tracker.reset(new SliceTracker(&context_));
  context_.event_tracker.reset(new EventTracker(&context_));
  context_.process_tracker.reset(new ProcessTracker(&context_));
  context_.syscall_tracker.reset(new SyscallTracker(&context_));
  context_.clock_tracker.reset(new ClockTracker(&context_));

  ArgsTable::RegisterTable(*db_, context_.storage.get());
  ProcessTable::RegisterTable(*db_, context_.storage.get());
  SchedSliceTable::RegisterTable(*db_, context_.storage.get());
  SliceTable::RegisterTable(*db_, context_.storage.get());
  SqlStatsTable::RegisterTable(*db_, context_.storage.get());
  StringTable::RegisterTable(*db_, context_.storage.get());
  ThreadTable::RegisterTable(*db_, context_.storage.get());
  CounterDefinitionsTable::RegisterTable(*db_, context_.storage.get());
  CounterValuesTable::RegisterTable(*db_, context_.storage.get());
  SpanJoinOperatorTable::RegisterTable(*db_, context_.storage.get());
  WindowOperatorTable::RegisterTable(*db_, context_.storage.get());
  InstantsTable::RegisterTable(*db_, context_.storage.get());
  StatsTable::RegisterTable(*db_, context_.storage.get());
  AndroidLogsTable::RegisterTable(*db_, context_.storage.get());
  RawTable::RegisterTable(*db_, context_.storage.get());
}

TraceProcessorImpl::~TraceProcessorImpl() {
  for (auto* it : iterators_)
    it->Reset();
}

bool TraceProcessorImpl::Parse(std::unique_ptr<uint8_t[]> data, size_t size) {
  if (size == 0)
    return true;
  if (unrecoverable_parse_error_)
    return false;

  // If this is the first Parse() call, guess the trace type and create the
  // appropriate parser.
  if (!context_.chunk_reader) {
    TraceType trace_type = GuessTraceType(data.get(), size);
    switch (trace_type) {
      case kJsonTraceType:
        PERFETTO_DLOG("Legacy JSON trace detected");
#if PERFETTO_BUILDFLAG(PERFETTO_STANDALONE_BUILD)
        context_.chunk_reader.reset(new JsonTraceTokenizer(&context_));
        context_.sorter.reset(
            new TraceSorter(&context_, std::numeric_limits<int64_t>::max()));
        context_.parser.reset(new JsonTraceParser(&context_));
#else
        PERFETTO_FATAL("JSON traces only supported in standalone mode.");
#endif
        break;
      case kProtoTraceType:
        context_.chunk_reader.reset(new ProtoTraceTokenizer(&context_));
        context_.sorter.reset(new TraceSorter(
            &context_, static_cast<int64_t>(cfg_.window_size_ns)));
        context_.parser.reset(new ProtoTraceParser(&context_));
        break;
      case kUnknownTraceType:
        return false;
    }
  }

  bool res = context_.chunk_reader->Parse(std::move(data), size);
  unrecoverable_parse_error_ |= !res;
  return res;
}

void TraceProcessorImpl::NotifyEndOfFile() {
  if (unrecoverable_parse_error_ || !context_.chunk_reader)
    return;

  context_.sorter->ExtractEventsForced();
  BuildBoundsTable(*db_, context_.storage->GetTraceTimestampBoundsNs());
}

TraceProcessor::Iterator TraceProcessorImpl::ExecuteQuery(
    const std::string& sql) {
  sqlite3_stmt* raw_stmt;
  int err = sqlite3_prepare_v2(*db_, sql.c_str(), static_cast<int>(sql.size()),
                               &raw_stmt, nullptr);
  base::Optional<std::string> error;
  uint32_t col_count = 0;
  if (err != SQLITE_OK) {
    error = sqlite3_errmsg(*db_);
  } else {
    col_count = static_cast<uint32_t>(sqlite3_column_count(raw_stmt));
  }
  std::unique_ptr<IteratorImpl> impl(
      new IteratorImpl(this, *db_, ScopedStmt(raw_stmt), col_count, error));
  iterators_.emplace_back(impl.get());
  return TraceProcessor::Iterator(std::move(impl));
}

void TraceProcessorImpl::InterruptQuery() {
  if (!db_)
    return;
  query_interrupted_.store(true);
  sqlite3_interrupt(db_.get());
}

int TraceProcessorImpl::ComputeMetric(
    const std::vector<std::string>& metric_names,
    std::vector<uint8_t>* metrics_proto) {
  perfetto::base::ignore_result(metric_names, metrics_proto);
  return 0;
}

TraceProcessor::IteratorImpl::IteratorImpl(TraceProcessorImpl* trace_processor,
                                           sqlite3* db,
                                           ScopedStmt stmt,
                                           uint32_t column_count,
                                           base::Optional<std::string> error)
    : trace_processor_(trace_processor),
      db_(db),
      stmt_(std::move(stmt)),
      column_count_(column_count),
      error_(error) {}

TraceProcessor::IteratorImpl::~IteratorImpl() {
  if (trace_processor_) {
    auto* its = &trace_processor_->iterators_;
    auto it = std::find(its->begin(), its->end(), this);
    PERFETTO_CHECK(it != its->end());
    its->erase(it);
  }
}

void TraceProcessor::IteratorImpl::Reset() {
  *this = IteratorImpl(nullptr, nullptr, ScopedStmt(), 0, base::nullopt);
}

}  // namespace trace_processor
}  // namespace perfetto
