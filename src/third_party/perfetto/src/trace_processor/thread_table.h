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

#ifndef SRC_TRACE_PROCESSOR_THREAD_TABLE_H_
#define SRC_TRACE_PROCESSOR_THREAD_TABLE_H_

#include <limits>
#include <memory>

#include "src/trace_processor/table.h"
#include "src/trace_processor/trace_storage.h"

namespace perfetto {
namespace trace_processor {

// The implementation of the SQLite table containing each unique process with
// the metadata for those processes.
class ThreadTable : public Table {
 public:
  enum Column { kUtid = 0, kUpid = 1, kName = 2, kTid = 3, kStartTs = 4 };

  static void RegisterTable(sqlite3* db, const TraceStorage* storage);

  ThreadTable(sqlite3*, const TraceStorage*);

  // Table implementation.
  base::Optional<Table::Schema> Init(int, const char* const*) override;
  std::unique_ptr<Table::Cursor> CreateCursor(const QueryConstraints&,
                                              sqlite3_value**) override;
  int BestIndex(const QueryConstraints&, BestIndexInfo*) override;

 private:
  class Cursor : public Table::Cursor {
   public:
    Cursor(const TraceStorage* storage,
           const QueryConstraints&,
           sqlite3_value**);

    // Implementation of Table::Cursor.
    int Next() override;
    int Eof() override;
    int Column(sqlite3_context*, int N) override;

   private:
    const TraceStorage* const storage_;
    UniqueTid min;
    UniqueTid max;
    UniqueTid current;
    bool desc;
  };

  const TraceStorage* const storage_;
};
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_THREAD_TABLE_H_
