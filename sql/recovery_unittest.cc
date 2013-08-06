// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_ignorer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

// Execute |sql|, and stringify the results with |column_sep| between
// columns and |row_sep| between rows.
// TODO(shess): Promote this to a central testing helper.
std::string ExecuteWithResults(sql::Connection* db,
                               const char* sql,
                               const char* column_sep,
                               const char* row_sep) {
  sql::Statement s(db->GetUniqueStatement(sql));
  std::string ret;
  while (s.Step()) {
    if (!ret.empty())
      ret += row_sep;
    for (int i = 0; i < s.ColumnCount(); ++i) {
      if (i > 0)
        ret += column_sep;
      ret += s.ColumnString(i);
    }
  }
  return ret;
}

// Dump consistent human-readable representation of the database
// schema.  For tables or indices, this will contain the sql command
// to create the table or index.  For certain automatic SQLite
// structures with no sql, the name is used.
std::string GetSchema(sql::Connection* db) {
  const char kSql[] =
      "SELECT COALESCE(sql, name) FROM sqlite_master ORDER BY 1";
  return ExecuteWithResults(db, kSql, "|", "\n");
}

int GetPageSize(sql::Connection* db) {
  sql::Statement s(db->GetUniqueStatement("PRAGMA page_size"));
  EXPECT_TRUE(s.Step());
  return s.ColumnInt(0);
}

// Get |name|'s root page number in the database.
int GetRootPage(sql::Connection* db, const char* name) {
  const char kPageSql[] = "SELECT rootpage FROM sqlite_master WHERE name = ?";
  sql::Statement s(db->GetUniqueStatement(kPageSql));
  s.BindString(0, name);
  EXPECT_TRUE(s.Step());
  return s.ColumnInt(0);
}

// Helper to read a SQLite page into a buffer.  |page_no| is 1-based
// per SQLite usage.
bool ReadPage(const base::FilePath& path, size_t page_no,
              char* buf, size_t page_size) {
  file_util::ScopedFILE file(file_util::OpenFile(path, "rb"));
  if (!file.get())
    return false;
  if (0 != fseek(file.get(), (page_no - 1) * page_size, SEEK_SET))
    return false;
  if (1u != fread(buf, page_size, 1, file.get()))
    return false;
  return true;
}

// Helper to write a SQLite page into a buffer.  |page_no| is 1-based
// per SQLite usage.
bool WritePage(const base::FilePath& path, size_t page_no,
               const char* buf, size_t page_size) {
  file_util::ScopedFILE file(file_util::OpenFile(path, "rb+"));
  if (!file.get())
    return false;
  if (0 != fseek(file.get(), (page_no - 1) * page_size, SEEK_SET))
    return false;
  if (1u != fwrite(buf, page_size, 1, file.get()))
    return false;
  return true;
}

class SQLRecoveryTest : public testing::Test {
 public:
  SQLRecoveryTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(db_path()));
  }

  virtual void TearDown() {
    db_.Close();
  }

  sql::Connection& db() { return db_; }

  base::FilePath db_path() {
    return temp_dir_.path().AppendASCII("SQLRecoveryTest.db");
  }

  bool Reopen() {
    db_.Close();
    return db_.Open(db_path());
  }

 private:
  base::ScopedTempDir temp_dir_;
  sql::Connection db_;
};

TEST_F(SQLRecoveryTest, RecoverBasic) {
  const char kCreateSql[] = "CREATE TABLE x (t TEXT)";
  const char kInsertSql[] = "INSERT INTO x VALUES ('This is a test')";
  ASSERT_TRUE(db().Execute(kCreateSql));
  ASSERT_TRUE(db().Execute(kInsertSql));
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db()));

  // If the Recovery handle goes out of scope without being
  // Recovered(), the database is razed.
  {
    scoped_ptr<sql::Recovery> recovery = sql::Recovery::Begin(&db(), db_path());
    ASSERT_TRUE(recovery.get());
  }
  EXPECT_FALSE(db().is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db().is_open());
  ASSERT_EQ("", GetSchema(&db()));

  // Recreate the database.
  ASSERT_TRUE(db().Execute(kCreateSql));
  ASSERT_TRUE(db().Execute(kInsertSql));
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db()));

  // Unrecoverable() also razes.
  {
    scoped_ptr<sql::Recovery> recovery = sql::Recovery::Begin(&db(), db_path());
    ASSERT_TRUE(recovery.get());
    sql::Recovery::Unrecoverable(recovery.Pass());

    // TODO(shess): Test that calls to recover.db() start failing.
  }
  EXPECT_FALSE(db().is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db().is_open());
  ASSERT_EQ("", GetSchema(&db()));

  // Recreate the database.
  ASSERT_TRUE(db().Execute(kCreateSql));
  ASSERT_TRUE(db().Execute(kInsertSql));
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db()));

  // Recovered() replaces the original with the "recovered" version.
  {
    scoped_ptr<sql::Recovery> recovery = sql::Recovery::Begin(&db(), db_path());
    ASSERT_TRUE(recovery.get());

    // Create the new version of the table.
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    // Insert different data to distinguish from original database.
    const char kAltInsertSql[] = "INSERT INTO x VALUES ('That was a test')";
    ASSERT_TRUE(recovery->db()->Execute(kAltInsertSql));

    // Successfully recovered.
    ASSERT_TRUE(sql::Recovery::Recovered(recovery.Pass()));
  }
  EXPECT_FALSE(db().is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db().is_open());
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db()));

  const char* kXSql = "SELECT * FROM x ORDER BY 1";
  ASSERT_EQ("That was a test",
            ExecuteWithResults(&db(), kXSql, "|", "\n"));
}

// The recovery virtual table is only supported for Chromium's SQLite.
#if !defined(USE_SYSTEM_SQLITE)

// Run recovery through its paces on a valid database.
TEST_F(SQLRecoveryTest, VirtualTable) {
  const char kCreateSql[] = "CREATE TABLE x (t TEXT)";
  ASSERT_TRUE(db().Execute(kCreateSql));
  ASSERT_TRUE(db().Execute("INSERT INTO x VALUES ('This is a test')"));
  ASSERT_TRUE(db().Execute("INSERT INTO x VALUES ('That was a test')"));

  // Successfully recover the database.
  {
    scoped_ptr<sql::Recovery> recovery = sql::Recovery::Begin(&db(), db_path());

    // Tables to recover original DB, now at [corrupt].
    const char kRecoveryCreateSql[] =
        "CREATE VIRTUAL TABLE temp.recover_x using recover("
        "  corrupt.x,"
        "  t TEXT STRICT"
        ")";
    ASSERT_TRUE(recovery->db()->Execute(kRecoveryCreateSql));

    // Re-create the original schema.
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    // Copy the data from the recovery tables to the new database.
    const char kRecoveryCopySql[] =
        "INSERT INTO x SELECT t FROM recover_x";
    ASSERT_TRUE(recovery->db()->Execute(kRecoveryCopySql));

    // Successfully recovered.
    ASSERT_TRUE(sql::Recovery::Recovered(recovery.Pass()));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db()));

  const char* kXSql = "SELECT * FROM x ORDER BY 1";
  ASSERT_EQ("That was a test\nThis is a test",
            ExecuteWithResults(&db(), kXSql, "|", "\n"));
}

void RecoveryCallback(sql::Connection* db, const base::FilePath& db_path,
                      int* record_error, int error, sql::Statement* stmt) {
  *record_error = error;

  // Clear the error callback to prevent reentrancy.
  db->reset_error_callback();

  scoped_ptr<sql::Recovery> recovery = sql::Recovery::Begin(db, db_path);
  ASSERT_TRUE(recovery.get());

  const char kRecoveryCreateSql[] =
      "CREATE VIRTUAL TABLE temp.recover_x using recover("
      "  corrupt.x,"
      "  id INTEGER STRICT,"
      "  v INTEGER STRICT"
      ")";
  const char kCreateTable[] = "CREATE TABLE x (id INTEGER, v INTEGER)";
  const char kCreateIndex[] = "CREATE UNIQUE INDEX x_id ON x (id)";

  // Replicate data over.
  const char kRecoveryCopySql[] =
      "INSERT OR REPLACE INTO x SELECT id, v FROM recover_x";

  ASSERT_TRUE(recovery->db()->Execute(kRecoveryCreateSql));
  ASSERT_TRUE(recovery->db()->Execute(kCreateTable));
  ASSERT_TRUE(recovery->db()->Execute(kCreateIndex));
  ASSERT_TRUE(recovery->db()->Execute(kRecoveryCopySql));

  ASSERT_TRUE(sql::Recovery::Recovered(recovery.Pass()));
}

// Build a database, corrupt it by making an index reference to
// deleted row, then recover when a query selects that row.
TEST_F(SQLRecoveryTest, RecoverCorruptIndex) {
  const char kCreateTable[] = "CREATE TABLE x (id INTEGER, v INTEGER)";
  const char kCreateIndex[] = "CREATE UNIQUE INDEX x_id ON x (id)";
  ASSERT_TRUE(db().Execute(kCreateTable));
  ASSERT_TRUE(db().Execute(kCreateIndex));

  // Insert a bit of data.
  {
    ASSERT_TRUE(db().BeginTransaction());

    const char kInsertSql[] = "INSERT INTO x (id, v) VALUES (?, ?)";
    sql::Statement s(db().GetUniqueStatement(kInsertSql));
    for (int i = 0; i < 10; ++i) {
      s.Reset(true);
      s.BindInt(0, i);
      s.BindInt(1, i);
      EXPECT_FALSE(s.Step());
      EXPECT_TRUE(s.Succeeded());
    }

    ASSERT_TRUE(db().CommitTransaction());
  }


  // Capture the index's root page into |buf|.
  int index_page = GetRootPage(&db(), "x_id");
  int page_size = GetPageSize(&db());
  scoped_ptr<char[]> buf(new char[page_size]);
  ASSERT_TRUE(ReadPage(db_path(), index_page, buf.get(), page_size));

  // Delete the row from the table and index.
  ASSERT_TRUE(db().Execute("DELETE FROM x WHERE id = 0"));

  // Close to clear any cached data.
  db().Close();

  // Put the stale index page back.
  ASSERT_TRUE(WritePage(db_path(), index_page, buf.get(), page_size));

  // At this point, the index references a value not in the table.

  ASSERT_TRUE(Reopen());

  int error = SQLITE_OK;
  db().set_error_callback(base::Bind(&RecoveryCallback,
                                     &db(), db_path(), &error));

  // This works before the callback is called.
  const char kTrivialSql[] = "SELECT COUNT(*) FROM sqlite_master";
  EXPECT_TRUE(db().IsSQLValid(kTrivialSql));

  // TODO(shess): Could this be delete?  Anything which fails should work.
  const char kSelectSql[] = "SELECT v FROM x WHERE id = 0";
  ASSERT_FALSE(db().Execute(kSelectSql));
  EXPECT_EQ(SQLITE_CORRUPT, error);

  // Database handle has been poisoned.
  EXPECT_FALSE(db().IsSQLValid(kTrivialSql));

  ASSERT_TRUE(Reopen());

  // The recovered table should reflect the deletion.
  const char kSelectAllSql[] = "SELECT v FROM x ORDER BY id";
  EXPECT_EQ("1,2,3,4,5,6,7,8,9",
            ExecuteWithResults(&db(), kSelectAllSql, "|", ","));

  // The failing statement should now succeed, with no results.
  EXPECT_EQ("", ExecuteWithResults(&db(), kSelectSql, "|", ","));
}

// Build a database, corrupt it by making a table contain a row not
// referenced by the index, then recover the database.
TEST_F(SQLRecoveryTest, RecoverCorruptTable) {
  const char kCreateTable[] = "CREATE TABLE x (id INTEGER, v INTEGER)";
  const char kCreateIndex[] = "CREATE UNIQUE INDEX x_id ON x (id)";
  ASSERT_TRUE(db().Execute(kCreateTable));
  ASSERT_TRUE(db().Execute(kCreateIndex));

  // Insert a bit of data.
  {
    ASSERT_TRUE(db().BeginTransaction());

    const char kInsertSql[] = "INSERT INTO x (id, v) VALUES (?, ?)";
    sql::Statement s(db().GetUniqueStatement(kInsertSql));
    for (int i = 0; i < 10; ++i) {
      s.Reset(true);
      s.BindInt(0, i);
      s.BindInt(1, i);
      EXPECT_FALSE(s.Step());
      EXPECT_TRUE(s.Succeeded());
    }

    ASSERT_TRUE(db().CommitTransaction());
  }

  // Capture the table's root page into |buf|.
  // Find the page the table is stored on.
  const int table_page = GetRootPage(&db(), "x");
  const int page_size = GetPageSize(&db());
  scoped_ptr<char[]> buf(new char[page_size]);
  ASSERT_TRUE(ReadPage(db_path(), table_page, buf.get(), page_size));

  // Delete the row from the table and index.
  ASSERT_TRUE(db().Execute("DELETE FROM x WHERE id = 0"));

  // Close to clear any cached data.
  db().Close();

  // Put the stale table page back.
  ASSERT_TRUE(WritePage(db_path(), table_page, buf.get(), page_size));

  // At this point, the table contains a value not referenced by the
  // index.
  // TODO(shess): Figure out a query which causes SQLite to notice
  // this organically.  Meanwhile, just handle it manually.

  ASSERT_TRUE(Reopen());

  // Index shows one less than originally inserted.
  const char kCountSql[] = "SELECT COUNT (*) FROM x";
  EXPECT_EQ("9", ExecuteWithResults(&db(), kCountSql, "|", ","));

  // A full table scan shows all of the original data.
  const char kDistinctSql[] = "SELECT DISTINCT COUNT (id) FROM x";
  EXPECT_EQ("10", ExecuteWithResults(&db(), kDistinctSql, "|", ","));

  // Insert id 0 again.  Since it is not in the index, the insert
  // succeeds, but results in a duplicate value in the table.
  const char kInsertSql[] = "INSERT INTO x (id, v) VALUES (0, 100)";
  ASSERT_TRUE(db().Execute(kInsertSql));

  // Duplication is visible.
  EXPECT_EQ("10", ExecuteWithResults(&db(), kCountSql, "|", ","));
  EXPECT_EQ("11", ExecuteWithResults(&db(), kDistinctSql, "|", ","));

  // This works before the callback is called.
  const char kTrivialSql[] = "SELECT COUNT(*) FROM sqlite_master";
  EXPECT_TRUE(db().IsSQLValid(kTrivialSql));

  // Call the recovery callback manually.
  int error = SQLITE_OK;
  RecoveryCallback(&db(), db_path(), &error, SQLITE_CORRUPT, NULL);
  EXPECT_EQ(SQLITE_CORRUPT, error);

  // Database handle has been poisoned.
  EXPECT_FALSE(db().IsSQLValid(kTrivialSql));

  ASSERT_TRUE(Reopen());

  // The recovered table has consistency between the index and the table.
  EXPECT_EQ("10", ExecuteWithResults(&db(), kCountSql, "|", ","));
  EXPECT_EQ("10", ExecuteWithResults(&db(), kDistinctSql, "|", ","));

  // The expected value was retained.
  const char kSelectSql[] = "SELECT v FROM x WHERE id = 0";
  EXPECT_EQ("100", ExecuteWithResults(&db(), kSelectSql, "|", ","));
}
#endif  // !defined(USE_SYSTEM_SQLITE)

}  // namespace
