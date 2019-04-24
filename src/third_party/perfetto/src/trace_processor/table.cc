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

#include "src/trace_processor/table.h"

#include <ctype.h>
#include <string.h>
#include <algorithm>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

namespace {

struct TableDescriptor {
  Table::Factory factory;
  const TraceStorage* storage = nullptr;
  std::string name;
  sqlite3_module module = {};
};

Table* ToTable(sqlite3_vtab* vtab) {
  return static_cast<Table*>(vtab);
}

Table::RawCursor* ToCursor(sqlite3_vtab_cursor* cursor) {
  return static_cast<Table::RawCursor*>(cursor);
}

std::string TypeToString(Table::ColumnType type) {
  switch (type) {
    case Table::ColumnType::kString:
      return "STRING";
    case Table::ColumnType::kUint:
      return "UNSIGNED INT";
    case Table::ColumnType::kLong:
      return "BIG INT";
    case Table::ColumnType::kInt:
      return "INT";
    case Table::ColumnType::kDouble:
      return "DOUBLE";
    case Table::ColumnType::kUnknown:
      PERFETTO_FATAL("Cannot map unknown column type");
  }
  PERFETTO_FATAL("Not reached");  // For gcc
}

}  // namespace

// static
bool Table::debug = false;

Table::Table() = default;
Table::~Table() = default;

void Table::RegisterInternal(sqlite3* db,
                             const TraceStorage* storage,
                             const std::string& table_name,
                             bool read_write,
                             bool requires_args,
                             Factory factory) {
  std::unique_ptr<TableDescriptor> desc(new TableDescriptor());
  desc->storage = storage;
  desc->factory = factory;
  desc->name = table_name;
  sqlite3_module* module = &desc->module;
  memset(module, 0, sizeof(*module));

  auto create_fn = [](sqlite3* xdb, void* arg, int argc,
                      const char* const* argv, sqlite3_vtab** tab, char**) {
    const TableDescriptor* xdesc = static_cast<const TableDescriptor*>(arg);
    auto table = xdesc->factory(xdb, xdesc->storage);

    auto opt_schema = table->Init(argc, argv);
    if (!opt_schema.has_value()) {
      PERFETTO_ELOG("Failed to create schema (table %s)", xdesc->name.c_str());
      return SQLITE_ERROR;
    }

    const auto& schema = opt_schema.value();
    auto create_stmt = schema.ToCreateTableStmt();
    PERFETTO_DLOG("Create table statement: %s", create_stmt.c_str());

    int res = sqlite3_declare_vtab(xdb, create_stmt.c_str());
    if (res != SQLITE_OK)
      return res;

    // Freed in xDisconnect().
    table->schema_ = std::move(schema);
    table->name_ = xdesc->name;
    *tab = table.release();

    return SQLITE_OK;
  };
  module->xCreate = create_fn;
  module->xConnect = create_fn;

  auto destroy_fn = [](sqlite3_vtab* t) {
    delete ToTable(t);
    return SQLITE_OK;
  };
  module->xDisconnect = destroy_fn;
  module->xDestroy = destroy_fn;

  module->xOpen = [](sqlite3_vtab* t, sqlite3_vtab_cursor** c) {
    return ToTable(t)->OpenInternal(c);
  };

  module->xClose = [](sqlite3_vtab_cursor* c) {
    delete ToCursor(c);
    return SQLITE_OK;
  };

  module->xBestIndex = [](sqlite3_vtab* t, sqlite3_index_info* i) {
    return ToTable(t)->BestIndexInternal(i);
  };

  module->xFilter = [](sqlite3_vtab_cursor* c, int i, const char* s, int a,
                       sqlite3_value** v) {
    return ToCursor(c)->Filter(i, s, a, v);
  };
  module->xNext = [](sqlite3_vtab_cursor* c) {
    return ToCursor(c)->cursor()->Next();
  };
  module->xEof = [](sqlite3_vtab_cursor* c) {
    return ToCursor(c)->cursor()->Eof();
  };
  module->xColumn = [](sqlite3_vtab_cursor* c, sqlite3_context* a, int b) {
    return ToCursor(c)->cursor()->Column(a, b);
  };

  module->xRowid = [](sqlite3_vtab_cursor* c, sqlite3_int64* r) {
    return ToCursor(c)->cursor()->RowId(r);
  };

  module->xFindFunction =
      [](sqlite3_vtab* t, int, const char* name,
         void (**fn)(sqlite3_context*, int, sqlite3_value**),
         void** args) { return ToTable(t)->FindFunction(name, fn, args); };

  if (read_write) {
    module->xUpdate = [](sqlite3_vtab* t, int a, sqlite3_value** v,
                         sqlite3_int64* r) {
      return ToTable(t)->Update(a, v, r);
    };
  }

  int res = sqlite3_create_module_v2(
      db, table_name.c_str(), module, desc.release(),
      [](void* arg) { delete static_cast<TableDescriptor*>(arg); });
  PERFETTO_CHECK(res == SQLITE_OK);

  // Register virtual tables into an internal 'perfetto_tables' table. This is
  // used for iterating through all the tables during a database export. Note
  // that virtual tables requiring arguments aren't registered because they
  // can't be automatically instantiated for exporting.
  if (!requires_args) {
    char* insert_sql = sqlite3_mprintf(
        "INSERT INTO perfetto_tables(name) VALUES('%q')", table_name.c_str());
    char* error = nullptr;
    sqlite3_exec(db, insert_sql, 0, 0, &error);
    sqlite3_free(insert_sql);
    if (error) {
      PERFETTO_ELOG("Error registering table: %s", error);
      sqlite3_free(error);
    }
  }
}

int Table::OpenInternal(sqlite3_vtab_cursor** ppCursor) {
  // Freed in xClose().
  *ppCursor = static_cast<sqlite3_vtab_cursor*>(new RawCursor(this));
  return SQLITE_OK;
}

int Table::BestIndexInternal(sqlite3_index_info* idx) {
  QueryConstraints query_constraints;

  for (int i = 0; i < idx->nOrderBy; i++) {
    int column = idx->aOrderBy[i].iColumn;
    bool desc = idx->aOrderBy[i].desc;
    query_constraints.AddOrderBy(column, desc);
  }

  for (int i = 0; i < idx->nConstraint; i++) {
    const auto& cs = idx->aConstraint[i];
    if (!cs.usable)
      continue;
    query_constraints.AddConstraint(cs.iColumn, cs.op);

    // argvIndex is 1-based so use the current size of the vector.
    int argv_index = static_cast<int>(query_constraints.constraints().size());
    idx->aConstraintUsage[i].argvIndex = argv_index;
  }

  BestIndexInfo info;
  info.omit.resize(query_constraints.constraints().size());

  int ret = BestIndex(query_constraints, &info);

  if (Table::debug) {
    PERFETTO_LOG(
        "[%s::BestIndex] constraints=%s orderByConsumed=%d estimatedCost=%d",
        name_.c_str(), query_constraints.ToNewSqlite3String().get(),
        info.order_by_consumed, info.estimated_cost);
  }

  if (ret != SQLITE_OK)
    return ret;

  idx->orderByConsumed = info.order_by_consumed;
  idx->estimatedCost = info.estimated_cost;

  size_t j = 0;
  for (int i = 0; i < idx->nConstraint; i++) {
    const auto& cs = idx->aConstraint[i];
    if (cs.usable)
      idx->aConstraintUsage[i].omit = info.omit[j++];
  }

  if (!info.order_by_consumed)
    query_constraints.ClearOrderBy();

  idx->idxStr = query_constraints.ToNewSqlite3String().release();
  idx->needToFreeIdxStr = true;
  idx->idxNum = ++best_index_num_;

  return SQLITE_OK;
}

int Table::FindFunction(const char*, FindFunctionFn, void**) {
  return 0;
}

int Table::Update(int, sqlite3_value**, sqlite3_int64*) {
  return SQLITE_READONLY;
}

Table::RawCursor::RawCursor(Table* table) : table_(table) {}

int Table::RawCursor::Filter(int idxNum,
                             const char* idxStr,
                             int argc,
                             sqlite3_value** argv) {
  auto* table = ToTable(this->pVtab);
  bool cache_hit = true;
  if (idxNum != table->qc_hash_) {
    table->qc_cache_ = QueryConstraints::FromString(idxStr);
    table->qc_hash_ = idxNum;
    cache_hit = false;
  }
  if (Table::debug) {
    PERFETTO_LOG("[%s::Filter] constraints=%s argc=%d cache_hit=%d",
                 table->name_.c_str(), idxStr, argc, cache_hit);
  }
  PERFETTO_DCHECK(table->qc_cache_.constraints().size() ==
                  static_cast<size_t>(argc));
  cursor_ = table_->CreateCursor(table->qc_cache_, argv);
  return !cursor_ ? SQLITE_ERROR : SQLITE_OK;
}

Table::Cursor::~Cursor() = default;

int Table::Cursor::RowId(sqlite3_int64*) {
  return SQLITE_ERROR;
}

Table::Column::Column(size_t index,
                      std::string name,
                      ColumnType type,
                      bool hidden)
    : index_(index), name_(name), type_(type), hidden_(hidden) {}

Table::Schema::Schema(std::vector<Column> columns,
                      std::vector<size_t> primary_keys)
    : columns_(std::move(columns)), primary_keys_(std::move(primary_keys)) {
  for (size_t i = 0; i < columns_.size(); i++) {
    PERFETTO_CHECK(columns_[i].index() == i);
  }
  for (auto key : primary_keys_) {
    PERFETTO_CHECK(key < columns_.size());
  }
}

Table::Schema::Schema() = default;
Table::Schema::Schema(const Schema&) = default;
Table::Schema& Table::Schema::operator=(const Schema&) = default;

std::string Table::Schema::ToCreateTableStmt() const {
  std::string stmt = "CREATE TABLE x(";
  for (size_t i = 0; i < columns_.size(); ++i) {
    const Column& col = columns_[i];
    stmt += " " + col.name();

    if (col.type() != ColumnType::kUnknown) {
      stmt += " " + TypeToString(col.type());
    } else if (std::find(primary_keys_.begin(), primary_keys_.end(), i) !=
               primary_keys_.end()) {
      PERFETTO_FATAL("Unknown type for primary key column %s",
                     col.name().c_str());
    }
    if (col.hidden()) {
      stmt += " HIDDEN";
    }
    stmt += ",";
  }
  stmt += " PRIMARY KEY(";
  for (size_t i = 0; i < primary_keys_.size(); i++) {
    if (i != 0)
      stmt += ", ";
    stmt += columns_[primary_keys_[i]].name();
  }
  stmt += ")) WITHOUT ROWID;";
  return stmt;
}

}  // namespace trace_processor
}  // namespace perfetto
