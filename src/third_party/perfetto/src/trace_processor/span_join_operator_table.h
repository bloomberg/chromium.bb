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

#ifndef SRC_TRACE_PROCESSOR_SPAN_JOIN_OPERATOR_TABLE_H_
#define SRC_TRACE_PROCESSOR_SPAN_JOIN_OPERATOR_TABLE_H_

#include <sqlite3.h>
#include <array>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/trace_processor/scoped_db.h"
#include "src/trace_processor/table.h"

namespace perfetto {
namespace trace_processor {

// Implements the SPAN JOIN operation between two tables on a particular column.
//
// Span:
// A span is a row with a timestamp and a duration. It can is used to model
// operations which run for a particular *span* of time.
//
// We draw spans like so (time on the x-axis):
// start of span->[ time where opertion is running ]<- end of span
//
// Multiple spans can happen in parallel:
// [      ]
//    [        ]
//   [                    ]
//  [ ]
//
// The above for example, models scheduling activity on a 4-core computer for a
// short period of time.
//
// Span join:
// The span join operation can be thought of as the intersection of span tables.
// That is, the join table has a span for each pair of spans in the child tables
// where the spans overlap. Because many spans are possible in parallel, an
// extra metadata column (labelled the "join column") is used to distinguish
// between the spanned tables.
//
// For a given join key suppose these were the two span tables:
// Table 1:   [        ]              [      ]         [ ]
// Table 2:          [      ]            [  ]           [      ]
// Output :          [ ]                 [  ]           []
//
// All other columns apart from timestamp (ts), duration (dur) and the join key
// are passed through unchanged.
class SpanJoinOperatorTable : public Table {
 public:
  // Columns of the span operator table.
  enum Column {
    kTimestamp = 0,
    kDuration = 1,
    kPartition = 2,
    // All other columns are dynamic depending on the joined tables.
  };

  SpanJoinOperatorTable(sqlite3*, const TraceStorage*);

  static void RegisterTable(sqlite3* db, const TraceStorage* storage);

  // Table implementation.
  base::Optional<Table::Schema> Init(int, const char* const*) override;
  std::unique_ptr<Table::Cursor> CreateCursor(const QueryConstraints&,
                                              sqlite3_value**) override;
  int BestIndex(const QueryConstraints& qc, BestIndexInfo* info) override;

 private:
  enum class PartitioningType {
    kNoPartitioning = 0,
    kSamePartitioning = 1,
    kMixedPartitioning = 2
  };

  // Parsed version of a table descriptor.
  struct TableDescriptor {
    static base::Optional<TableDescriptor> Parse(
        const std::string& raw_descriptor);

    std::string name;
    std::string partition_col;
  };

  // Contains the definition of the child tables.
  class TableDefinition {
   public:
    TableDefinition() = default;

    TableDefinition(std::string name,
                    std::string partition_col,
                    std::vector<Table::Column> cols,
                    uint32_t ts_idx,
                    uint32_t dur_idx,
                    uint32_t partition_idx);

    const std::string& name() const { return name_; }
    const std::string& partition_col() const { return partition_col_; }
    const std::vector<Table::Column>& columns() const { return cols_; }

    uint32_t ts_idx() const { return ts_idx_; }
    uint32_t dur_idx() const { return dur_idx_; }
    uint32_t partition_idx() const { return partition_idx_; }

    bool IsPartitioned() const { return !partition_col_.empty(); }

   private:
    std::string name_;
    std::string partition_col_;
    std::vector<Table::Column> cols_;
    uint32_t ts_idx_ = std::numeric_limits<uint32_t>::max();
    uint32_t dur_idx_ = std::numeric_limits<uint32_t>::max();
    uint32_t partition_idx_ = std::numeric_limits<uint32_t>::max();
  };

  // Base class for a cursor on the span table.
  class Cursor : public Table::Cursor {
   public:
    Cursor(SpanJoinOperatorTable*, sqlite3* db);
    ~Cursor() override = default;

    int Initialize(const QueryConstraints& qc, sqlite3_value** argv);
    int Column(sqlite3_context* context, int N) override;

   protected:
    // State of a query on a particular table.
    class TableQueryState {
     public:
      TableQueryState(SpanJoinOperatorTable*,
                      const TableDefinition*,
                      sqlite3* db);

      int Initialize(const QueryConstraints& qc, sqlite3_value** argv);
      int StepAndCacheValues();
      void ReportSqliteResult(sqlite3_context* context, size_t index);
      int PrepareRawStmt();

      const TableDefinition* definition() const { return defn_; }

      int64_t ts_start() const { return ts_start_; }
      int64_t ts_end() const { return ts_end_; }
      int64_t partition() const { return partition_; }
      bool Eof() const {
        return ts_start_ == std::numeric_limits<int64_t>::max();
      }

     private:
      std::string CreateSqlQuery(const std::vector<std::string>& cs) const;

      std::string sql_query_;
      ScopedStmt stmt_;

      int64_t ts_start_ = std::numeric_limits<int64_t>::max();
      int64_t ts_end_ = std::numeric_limits<int64_t>::max();
      int64_t partition_ = std::numeric_limits<int64_t>::max();

      const TableDefinition* const defn_;
      sqlite3* const db_;
      SpanJoinOperatorTable* const table_;
    };

    static bool IsOverlappingSpan(TableQueryState* t1,
                                  TableQueryState* t2,
                                  TableQueryState** next_stepped_table);

    TableQueryState t1_;
    TableQueryState t2_;
    TableQueryState* next_stepped_table_ = nullptr;

    SpanJoinOperatorTable* const table_;
  };

  class SinglePartitioningCursor : public Cursor {
   public:
    SinglePartitioningCursor(SpanJoinOperatorTable*, sqlite3* db);
    ~SinglePartitioningCursor() override = default;

    int Next() override;
    int Eof() override;
  };

  class MixedPartitioningCursor : public Cursor {
   public:
    MixedPartitioningCursor(SpanJoinOperatorTable*, sqlite3* db);
    ~MixedPartitioningCursor() override = default;

    int Next() override;
    int Eof() override;
  };

  // Identifier for a column by index in a given table.
  struct ColumnLocator {
    const TableDefinition* defn;
    size_t col_index;
  };

  base::Optional<TableDefinition> CreateTableDefinition(
      const TableDescriptor& desc);

  std::vector<std::string> ComputeSqlConstraintsForDefinition(
      const TableDefinition& defn,
      const QueryConstraints& qc,
      sqlite3_value** argv);

  std::string GetNameForGlobalColumnIndex(const TableDefinition& defn,
                                          int global_column);

  void CreateSchemaColsForDefn(const TableDefinition& defn,
                               std::vector<Table::Column>* cols);

  TableDefinition t1_defn_;
  TableDefinition t2_defn_;
  PartitioningType partitioning_;
  std::unordered_map<size_t, ColumnLocator> global_index_to_column_locator_;

  sqlite3* const db_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SPAN_JOIN_OPERATOR_TABLE_H_
