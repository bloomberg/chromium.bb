// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_TEST_HELPERS_H_
#define SQL_TEST_TEST_HELPERS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

// Collection of test-only convenience functions.

namespace base {
class FilePath;
}

namespace sql {
class Connection;
}

namespace sql {
namespace test {

// SQLite stores the database size in the header, and if the actual
// OS-derived size is smaller, the database is considered corrupt.
// [This case is actually a common form of corruption in the wild.]
// This helper sets the in-header size to one page larger than the
// actual file size.  The resulting file will return SQLITE_CORRUPT
// for most operations unless PRAGMA writable_schema is turned ON.
//
// Returns false if any error occurs accessing the file.
bool CorruptSizeInHeader(const base::FilePath& db_path) WARN_UNUSED_RESULT;

// Return the number of tables in sqlite_master.
size_t CountSQLTables(sql::Connection* db) WARN_UNUSED_RESULT;

// Return the number of indices in sqlite_master.
size_t CountSQLIndices(sql::Connection* db) WARN_UNUSED_RESULT;

// Returns the number of columns in the named table.  0 indicates an
// error (probably no such table).
size_t CountTableColumns(sql::Connection* db, const char* table)
    WARN_UNUSED_RESULT;

// Sets |*count| to the number of rows in |table|.  Returns false in
// case of error, such as the table not existing.
bool CountTableRows(sql::Connection* db, const char* table, size_t* count);

// Creates a SQLite database at |db_path| from the sqlite .dump output
// at |sql_path|.  Returns false if |db_path| already exists, or if
// sql_path does not exist or cannot be read, or if there is an error
// executing the statements.
bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                           const base::FilePath& sql_path) WARN_UNUSED_RESULT;

// Return the results of running "PRAGMA integrity_check" on |db|.
// TODO(shess): sql::Connection::IntegrityCheck() is basically the
// same, but not as convenient for testing.  Maybe combine.
std::string IntegrityCheck(sql::Connection* db) WARN_UNUSED_RESULT;

}  // namespace test
}  // namespace sql

#endif  // SQL_TEST_TEST_HELPERS_H_
