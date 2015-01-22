// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

// Test that certain features are/are-not enabled in our SQLite.

namespace {

void CaptureErrorCallback(int* error_pointer, std::string* sql_text,
                          int error, sql::Statement* stmt) {
  *error_pointer = error;
  const char* text = stmt ? stmt->GetSQLStatement() : NULL;
  *sql_text = text ? text : "no statement available";
}

class SQLiteFeaturesTest : public testing::Test {
 public:
  SQLiteFeaturesTest() : error_(SQLITE_OK) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(temp_dir_.path().AppendASCII("SQLStatementTest.db")));

    // The error delegate will set |error_| and |sql_text_| when any sqlite
    // statement operation returns an error code.
    db_.set_error_callback(base::Bind(&CaptureErrorCallback,
                                      &error_, &sql_text_));
  }

  void TearDown() override {
    // If any error happened the original sql statement can be found in
    // |sql_text_|.
    EXPECT_EQ(SQLITE_OK, error_) << sql_text_;
    db_.Close();
  }

  void VerifyAndClearLastError(int expected_error) {
    EXPECT_EQ(expected_error, error_);
    error_ = SQLITE_OK;
    sql_text_.clear();
  }

  sql::Connection& db() { return db_; }

 private:
  base::ScopedTempDir temp_dir_;
  sql::Connection db_;

  // The error code of the most recent error.
  int error_;
  // Original statement which has caused the error.
  std::string sql_text_;
};

// Do not include fts1 support, it is not useful, and nobody is
// looking at it.
TEST_F(SQLiteFeaturesTest, NoFTS1) {
  ASSERT_EQ(SQLITE_ERROR, db().ExecuteAndReturnErrorCode(
      "CREATE VIRTUAL TABLE foo USING fts1(x)"));
}

#if defined(SQLITE_ENABLE_FTS2)
// fts2 is used for older history files, so we're signed on for keeping our
// version up-to-date.
// TODO(shess): Think up a crazy way to get out from having to support
// this forever.
TEST_F(SQLiteFeaturesTest, FTS2) {
  ASSERT_TRUE(db().Execute("CREATE VIRTUAL TABLE foo USING fts2(x)"));
}

// A standard SQLite will not include our patch.  This includes iOS.
#if !defined(USE_SYSTEM_SQLITE)
// Chromium fts2 was patched to treat "foo*" as a prefix search, though the icu
// tokenizer will return it as two tokens {"foo", "*"}.
TEST_F(SQLiteFeaturesTest, FTS2_Prefix) {
  const char kCreateSql[] =
      "CREATE VIRTUAL TABLE foo USING fts2(x, tokenize icu)";
  ASSERT_TRUE(db().Execute(kCreateSql));

  ASSERT_TRUE(db().Execute("INSERT INTO foo (x) VALUES ('test')"));

  sql::Statement s(db().GetUniqueStatement(
      "SELECT x FROM foo WHERE x MATCH 'te*'"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ("test", s.ColumnString(0));
}
#endif
#endif

// fts3 is used for current history files, and also for WebDatabase.
TEST_F(SQLiteFeaturesTest, FTS3) {
  ASSERT_TRUE(db().Execute("CREATE VIRTUAL TABLE foo USING fts3(x)"));
}

#if !defined(USE_SYSTEM_SQLITE)
// Test that fts3 doesn't need fts2's patch (see above).
TEST_F(SQLiteFeaturesTest, FTS3_Prefix) {
  const char kCreateSql[] =
      "CREATE VIRTUAL TABLE foo USING fts3(x, tokenize icu)";
  ASSERT_TRUE(db().Execute(kCreateSql));

  ASSERT_TRUE(db().Execute("INSERT INTO foo (x) VALUES ('test')"));

  sql::Statement s(db().GetUniqueStatement(
      "SELECT x FROM foo WHERE x MATCH 'te*'"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ("test", s.ColumnString(0));
}
#endif

#if !defined(USE_SYSTEM_SQLITE)
// Verify that Chromium's SQLite is compiled with HAVE_USLEEP defined.  With
// HAVE_USLEEP, SQLite uses usleep() with millisecond granularity.  Otherwise it
// uses sleep() with second granularity.
TEST_F(SQLiteFeaturesTest, UsesUsleep) {
  base::TimeTicks before = base::TimeTicks::Now();
  sqlite3_sleep(1);
  base::TimeDelta delta = base::TimeTicks::Now() - before;

  // It is not impossible for this to be over 1000 if things are compiled the
  // right way.  But it is very unlikely, most platforms seem to be around
  // <TBD>.
  LOG(ERROR) << "Milliseconds: " << delta.InMilliseconds();
  EXPECT_LT(delta.InMilliseconds(), 1000);
}
#endif

// Ensure that our SQLite version has working foreign key support with cascade
// delete support.
TEST_F(SQLiteFeaturesTest, ForeignKeySupport) {
  ASSERT_TRUE(db().Execute("PRAGMA foreign_keys=1"));
  ASSERT_TRUE(db().Execute("CREATE TABLE parents (id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db().Execute(
      "CREATE TABLE children ("
      "    id INTEGER PRIMARY KEY,"
      "    pid INTEGER NOT NULL REFERENCES parents(id) ON DELETE CASCADE)"));

  // Inserting without a matching parent should fail with constraint violation.
  EXPECT_FALSE(db().Execute("INSERT INTO children VALUES (10, 1)"));
  VerifyAndClearLastError(SQLITE_CONSTRAINT);

  size_t rows;
  EXPECT_TRUE(sql::test::CountTableRows(&db(), "children", &rows));
  EXPECT_EQ(0u, rows);

  // Inserting with a matching parent should work.
  ASSERT_TRUE(db().Execute("INSERT INTO parents VALUES (1)"));
  EXPECT_TRUE(db().Execute("INSERT INTO children VALUES (11, 1)"));
  EXPECT_TRUE(db().Execute("INSERT INTO children VALUES (12, 1)"));
  EXPECT_TRUE(sql::test::CountTableRows(&db(), "children", &rows));
  EXPECT_EQ(2u, rows);

  // Deleting the parent should cascade, i.e., delete the children as well.
  ASSERT_TRUE(db().Execute("DELETE FROM parents"));
  EXPECT_TRUE(sql::test::CountTableRows(&db(), "children", &rows));
  EXPECT_EQ(0u, rows);
}

}  // namespace
