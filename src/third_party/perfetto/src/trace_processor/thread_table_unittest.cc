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

#include "src/trace_processor/thread_table.h"

#include "src/trace_processor/args_tracker.h"
#include "src/trace_processor/event_tracker.h"
#include "src/trace_processor/process_table.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/scoped_db.h"
#include "src/trace_processor/trace_processor_context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace perfetto {
namespace trace_processor {
namespace {

class ThreadTableUnittest : public ::testing::Test {
 public:
  ThreadTableUnittest() {
    sqlite3* db = nullptr;
    PERFETTO_CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    db_.reset(db);

    context_.storage.reset(new TraceStorage());
    context_.args_tracker.reset(new ArgsTracker(&context_));
    context_.process_tracker.reset(new ProcessTracker(&context_));
    context_.event_tracker.reset(new EventTracker(&context_));

    ThreadTable::RegisterTable(db_.get(), context_.storage.get());
    ProcessTable::RegisterTable(db_.get(), context_.storage.get());
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

  ~ThreadTableUnittest() override { context_.storage->ResetStorage(); }

 protected:
  TraceProcessorContext context_;
  ScopedDb db_;
  ScopedStmt stmt_;
};

TEST_F(ThreadTableUnittest, Select) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  uint32_t prev_state = 32;
  static const char kThreadName1[] = "thread1";
  static const char kThreadName2[] = "thread2";
  int32_t prio = 1024;

  context_.event_tracker->PushSchedSwitch(cpu, timestamp, /*tid=*/1,
                                          kThreadName2, prio, prev_state,
                                          /*tid=*/4, kThreadName1, prio);
  context_.event_tracker->PushSchedSwitch(cpu, timestamp + 1, /*tid=*/4,
                                          kThreadName1, prio, prev_state,
                                          /*tid=*/1, kThreadName2, prio);

  context_.process_tracker->UpdateProcess(2, base::nullopt, "test");
  context_.process_tracker->UpdateThread(4 /*tid*/, 2 /*pid*/);
  PrepareValidStatement("SELECT utid, upid, tid, name FROM thread where tid=4");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), 1 /* utid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 1 /* upid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 2), 4 /* tid */);
  ASSERT_STREQ(GetColumnAsText(3), kThreadName1);

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

TEST_F(ThreadTableUnittest, SelectWhere) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  uint32_t prev_state = 32;
  static const char kThreadName1[] = "thread1";
  static const char kThreadName2[] = "thread2";
  int32_t prio = 1024;

  context_.event_tracker->PushSchedSwitch(cpu, timestamp, /*tid=*/1,
                                          kThreadName2, prio, prev_state,
                                          /*tid=*/4, kThreadName1, prio);
  context_.event_tracker->PushSchedSwitch(cpu, timestamp + 1, /*tid=*/4,
                                          kThreadName1, prio, prev_state,
                                          /*tid=*/1, kThreadName2, prio);
  context_.event_tracker->PushSchedSwitch(cpu, timestamp + 2, /*tid=*/1,
                                          kThreadName2, prio, prev_state,
                                          /*tid=*/4, kThreadName1, prio);

  context_.process_tracker->UpdateProcess(2, base::nullopt, "test");
  context_.process_tracker->UpdateThread(4 /*tid*/, 2 /*pid*/);
  context_.process_tracker->UpdateThread(1 /*tid*/, 2 /*pid*/);
  PrepareValidStatement(
      "SELECT utid, upid, tid, name FROM thread where tid = 4");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 0), 1 /* utid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 1 /* upid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 2), 4 /* tid */);
  ASSERT_STREQ(GetColumnAsText(3), kThreadName1);

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

TEST_F(ThreadTableUnittest, JoinWithProcess) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  uint32_t prev_state = 32;
  static const char kThreadName1[] = "thread1";
  static const char kThreadName2[] = "thread2";
  int32_t prio = 1024;

  context_.event_tracker->PushSchedSwitch(cpu, timestamp, /*tid=*/1,
                                          kThreadName2, prio, prev_state,
                                          /*tid=*/4, kThreadName1, prio);
  context_.event_tracker->PushSchedSwitch(cpu, timestamp + 1, /*tid=*/4,
                                          kThreadName1, prio, prev_state,
                                          /*tid=*/1, kThreadName2, prio);

  // Also create a process for which we haven't seen any thread.
  context_.process_tracker->UpdateProcess(7, base::nullopt, "pid7");

  context_.process_tracker->UpdateProcess(2, base::nullopt, "pid2");
  context_.process_tracker->UpdateThread(/*tid=*/4, /*pid=*/2);

  PrepareValidStatement(
      "SELECT utid, thread.tid, thread.name, process.upid, process.pid, "
      "process.name FROM thread INNER JOIN process USING (upid) WHERE pid = 2 "
      "ORDER BY tid");

  // At this point we should see two threads bound to process with pid=2:

  // (1) The (implicitly created) main thread (tid=pid=2).
  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  auto utid_for_tid2 = sqlite3_column_int(*stmt_, 0);
  ASSERT_GT(utid_for_tid2, 0);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 2 /* tid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 4), 2 /* pid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 3), 2 /* upid */);
  ASSERT_EQ(GetColumnAsText(2), nullptr);  // No name seen for main thread.
  ASSERT_STREQ(GetColumnAsText(5), "pid2");

  // (2) A thread with tid=4.
  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_ROW);
  auto utid_for_tid4 = sqlite3_column_int(*stmt_, 0);
  ASSERT_GT(utid_for_tid4, 0);
  ASSERT_NE(utid_for_tid4, utid_for_tid2);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 1), 4 /* tid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 4), 2 /* pid */);
  ASSERT_EQ(sqlite3_column_int(*stmt_, 3), 2 /* upid */);
  ASSERT_STREQ(GetColumnAsText(2), kThreadName1);
  ASSERT_STREQ(GetColumnAsText(5), "pid2");

  ASSERT_EQ(sqlite3_step(*stmt_), SQLITE_DONE);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
