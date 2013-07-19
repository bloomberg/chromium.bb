// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  ASSERT_EQ(ExecuteWithResults(&db(), kXSql, "|", "\n"),
            "That was a test");
}

}  // namespace
