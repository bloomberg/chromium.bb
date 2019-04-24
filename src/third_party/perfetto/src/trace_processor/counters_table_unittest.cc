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

#include "src/trace_processor/counters_table.h"
#include "src/trace_processor/event_tracker.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/scoped_db.h"
#include "src/trace_processor/trace_processor_context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace perfetto {
namespace trace_processor {
namespace {

class CountersTableUnittest : public ::testing::Test {
 public:
  CountersTableUnittest() {
    sqlite3* db = nullptr;
    PERFETTO_CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    db_.reset(db);

    context_.storage.reset(new TraceStorage());
    context_.event_tracker.reset(new EventTracker(&context_));
    context_.process_tracker.reset(new ProcessTracker(&context_));

    CountersTable::RegisterTable(db_.get(), context_.storage.get());
  }

  void PrepareValidStatement(const std::string& sql) {
    int size = static_cast<int>(sql.size());
    sqlite3_stmt* stmt;
    ASSERT_EQ(sqlite3_prepare_v2(*db_, sql.c_str(), size, &stmt, nullptr),
              SQLITE_OK);
    stmt_.reset(stmt);
  }

  const char* GetColumnAsText(int colId) {
    return reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, colId));
  }

  ~CountersTableUnittest() override { context_.storage->ResetStorage(); }

 protected:
  TraceProcessorContext context_;
  ScopedDb db_;
  ScopedStmt stmt_;
};

TEST_F(CountersTableUnittest, SelectWhereCpu) {
  int64_t timestamp = 1000;
  uint32_t freq = 3000;

  context_.storage->mutable_counters()->AddCounter(
      timestamp, 1, freq, 1 /* cpu */, RefType::kRefCpuId);
  context_.storage->mutable_counters()->AddCounter(
      timestamp + 1, 1, freq + 1000, 1 /* cpu */, RefType::kRefCpuId);
  context_.storage->mutable_counters()->AddCounter(
      timestamp + 2, 1, freq + 2000, 2 /* cpu */, RefType::kRefCpuId);

  PrepareValidStatement(
      "SELECT ts, "
      "lead(ts) over (partition by name, ref order by ts) - ts as dur, "
      "value "
      "FROM counters where ref = 1");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), timestamp);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 1);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 2), freq);

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), timestamp + 1);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 0);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 2), freq + 1000);

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

TEST_F(CountersTableUnittest, GroupByFreq) {
  int64_t timestamp = 1000;
  uint32_t freq = 3000;
  uint32_t name_id = 1;

  context_.storage->mutable_counters()->AddCounter(
      timestamp, name_id, freq, 1 /* cpu */, RefType::kRefCpuId);
  context_.storage->mutable_counters()->AddCounter(
      timestamp + 1, name_id, freq + 1000, 1 /* cpu */, RefType::kRefCpuId);
  context_.storage->mutable_counters()->AddCounter(
      timestamp + 3, name_id, freq, 1 /* cpu */, RefType::kRefCpuId);

  PrepareValidStatement(
      "SELECT value, sum(dur) as dur_sum "
      "FROM ( "
      "select value, "
      "lead(ts) over (partition by name, ref order by ts) - ts as dur "
      "from counters"
      ") "
      "where value > 0 "
      "group by value "
      "order by dur_sum desc");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), freq + 1000);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 2);

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), freq);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 1);

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

TEST_F(CountersTableUnittest, UtidLookupUpid) {
  int64_t timestamp = 1000;
  uint32_t value = 3000;
  uint32_t name_id = 1;

  uint32_t utid = context_.process_tracker->UpdateThread(timestamp, 1, 0);

  context_.storage->mutable_counters()->AddCounter(
      timestamp, name_id, value, utid, RefType::kRefUtidLookupUpid);

  PrepareValidStatement("SELECT value, ref, ref_type FROM counters");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), value);
  ASSERT_EQ(sqlite3_column_type(*stmt_, 1), SQLITE_NULL);
  ASSERT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, 2)),
               "upid");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);

  // Simulate some other processes to avoid a situation where utid == upid so
  // we cannot assert correctly below.
  context_.process_tracker->UpdateProcess(4);
  context_.process_tracker->UpdateProcess(10);
  context_.process_tracker->UpdateProcess(11);

  auto* thread = context_.storage->GetMutableThread(utid);
  thread->upid = context_.process_tracker->UpdateProcess(1);

  PrepareValidStatement("SELECT value, ref, ref_type FROM counters");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), value);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), thread->upid.value());
  ASSERT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, 2)),
               "upid");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

TEST_F(CountersTableUnittest, UtidLookupUpidSort) {
  int64_t timestamp = 1000;
  uint32_t value = 3000;
  uint32_t name_id = 1;

  uint32_t utid_a = context_.process_tracker->UpdateThread(timestamp, 100, 0);
  uint32_t utid_b = context_.process_tracker->UpdateThread(timestamp, 200, 0);

  auto* thread_a = context_.storage->GetMutableThread(utid_a);
  thread_a->upid = context_.process_tracker->UpdateProcess(100);

  context_.storage->mutable_counters()->AddCounter(
      timestamp, name_id, value, utid_a, RefType::kRefUtidLookupUpid);
  context_.storage->mutable_counters()->AddCounter(
      timestamp + 1, name_id, value, utid_b, RefType::kRefUtidLookupUpid);

  PrepareValidStatement("SELECT ts, ref, ref_type FROM counters ORDER BY ref");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), timestamp + 1);
  ASSERT_EQ(sqlite3_column_type(*stmt_, 1), SQLITE_NULL);
  ASSERT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, 2)),
               "upid");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), timestamp);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), thread_a->upid.value());
  ASSERT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, 2)),
               "upid");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

TEST_F(CountersTableUnittest, RefColumnComparator) {
  int64_t timestamp = 1000;

  UniquePid upid = context_.process_tracker->UpdateProcess(100);
  // To ensure that upid and utid are not the same number
  UniqueTid no_upid_tid =
      context_.process_tracker->UpdateThread(timestamp, 101, 0);
  UniqueTid utid = context_.process_tracker->UpdateThread(timestamp, 102, 0);
  context_.storage->GetMutableThread(utid)->upid = upid;
  ASSERT_NE(upid, utid);

  uint32_t ctr_lookup_upid =
      static_cast<uint32_t>(context_.storage->mutable_counters()->AddCounter(
          timestamp, 0, 1 /* value */, utid, RefType::kRefUtidLookupUpid));

  uint32_t ctr_upid =
      static_cast<uint32_t>(context_.storage->mutable_counters()->AddCounter(
          timestamp, 0, 1 /* value */, upid, RefType::kRefUpid));

  uint32_t ctr_null_upid =
      static_cast<uint32_t>(context_.storage->mutable_counters()->AddCounter(
          timestamp, 0, 1 /* value */, no_upid_tid,
          RefType::kRefUtidLookupUpid));

  const auto& cs = context_.storage->counters();
  CountersTable::RefColumn ref_column("ref", &cs.refs(), &cs.types(),
                                      context_.storage.get());
  auto comparator = ref_column.Sort(QueryConstraints::OrderBy());
  // Lookup equality
  ASSERT_EQ(comparator(ctr_lookup_upid, ctr_upid), 0);

  // Null handling
  ASSERT_EQ(comparator(ctr_upid, ctr_null_upid), 1);
  ASSERT_EQ(comparator(ctr_null_upid, ctr_upid), -1);
  ASSERT_EQ(comparator(ctr_null_upid, ctr_null_upid), 0);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
