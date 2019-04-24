/*
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

#ifndef SRC_TRACE_PROCESSOR_STORAGE_SCHEMA_H_
#define SRC_TRACE_PROCESSOR_STORAGE_SCHEMA_H_

#include <algorithm>

#include "src/trace_processor/filtered_row_index.h"
#include "src/trace_processor/sqlite_utils.h"
#include "src/trace_processor/storage_columns.h"
#include "src/trace_processor/table.h"
#include "src/trace_processor/trace_storage.h"

namespace perfetto {
namespace trace_processor {

// Defines the schema for a table which is backed by concrete storage (i.e. does
// not generate data on the fly).
// Used by all tables which are backed by data in TraceStorage.
class StorageSchema {
 public:
  using Columns = std::vector<std::unique_ptr<StorageColumn>>;

  // Builder class for StorageSchema.
  class Builder {
   public:
    template <class T, class... Args>
    Builder& AddColumn(std::string column_name, Args&&... args) {
      columns_.emplace_back(new T(column_name, std::forward<Args>(args)...));
      return *this;
    }

    template <class T>
    Builder& AddNumericColumn(
        std::string column_name,
        const std::deque<T>* vals,
        const std::deque<std::vector<uint32_t>>* index = nullptr) {
      columns_.emplace_back(
          new NumericColumn<T>(column_name, vals, index, false, false));
      return *this;
    }

    template <class T>
    Builder& AddOrderedNumericColumn(std::string column_name,
                                     const std::deque<T>* vals) {
      columns_.emplace_back(
          new NumericColumn<T>(column_name, vals, nullptr, false, true));
      return *this;
    }

    template <class Id>
    Builder& AddStringColumn(std::string column_name,
                             const std::deque<Id>* ids,
                             const std::deque<std::string>* string_map) {
      columns_.emplace_back(new StringColumn<Id>(column_name, ids, string_map));
      return *this;
    }

    StorageSchema Build(std::vector<std::string> primary_keys) {
      return StorageSchema(std::move(columns_), std::move(primary_keys));
    }

   private:
    Columns columns_;
  };

  StorageSchema();
  StorageSchema(Columns columns, std::vector<std::string> primary_keys);

  Table::Schema ToTableSchema();

  size_t ColumnIndexFromName(const std::string& name) const;

  const StorageColumn& GetColumn(size_t idx) const { return *(columns_[idx]); }

  Columns* mutable_columns() { return &columns_; }

 private:
  friend class Builder;

  Columns columns_;
  std::vector<std::string> primary_keys_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_STORAGE_SCHEMA_H_
