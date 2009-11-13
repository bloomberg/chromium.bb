// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/databases_table.h"

namespace webkit_database {

class TestErrorDelegate : public sql::ErrorDelegate {
 public:
  virtual ~TestErrorDelegate() { }
  virtual int OnError(
      int error, sql::Connection* connection, sql::Statement* stmt) {
    return error;
  }
};

static void CheckDetailsAreEqual(const DatabaseDetails& d1,
                                 const DatabaseDetails& d2) {
  EXPECT_EQ(d1.origin_identifier, d2.origin_identifier);
  EXPECT_EQ(d1.database_name, d2.database_name);
  EXPECT_EQ(d1.description, d2.description);
  EXPECT_EQ(d1.estimated_size, d2.estimated_size);
}

static bool DatabasesTableIsEmpty(sql::Connection* db) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*) FROM Databases"));
  return (statement.is_valid() && statement.Step() && !statement.ColumnInt(0));
}

TEST(DatabasesTableTest, TestIt) {
  // Initialize the 'Databases' table.
  ScopedTempDir temp_dir;
  sql::Connection db;

  // Set an error delegate that will make all operations return false on error.
  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate());
  db.set_error_delegate(error_delegate);

  // Initialize the temp dir and the 'Databases' table.
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(db.Open(temp_dir.path().Append(FILE_PATH_LITERAL("tracker.db"))));
  DatabasesTable databases_table(&db);
  EXPECT_TRUE(databases_table.Init());

  // The 'Databases' table should be empty.
  EXPECT_TRUE(DatabasesTableIsEmpty(&db));

  // Create the details for a databases.
  DatabaseDetails details_in1;
  DatabaseDetails details_out1;
  details_in1.origin_identifier = ASCIIToUTF16("origin1");
  details_in1.database_name = ASCIIToUTF16("db1");
  details_in1.description = ASCIIToUTF16("description_db1");
  details_in1.estimated_size = 100;

  // Updating details for this database should fail.
  EXPECT_FALSE(databases_table.UpdateDatabaseDetails(details_in1));
  EXPECT_FALSE(databases_table.GetDatabaseDetails(
      details_in1.origin_identifier,
      details_in1.database_name,
      &details_out1));

  // Inserting details for this database should pass.
  EXPECT_TRUE(databases_table.InsertDatabaseDetails(details_in1));
  EXPECT_TRUE(databases_table.GetDatabaseDetails(
      details_in1.origin_identifier,
      details_in1.database_name,
      &details_out1));
  EXPECT_EQ(1, databases_table.GetDatabaseID(details_in1.origin_identifier,
                                             details_in1.database_name));

  // Check that the details were correctly written to the database.
  CheckDetailsAreEqual(details_in1, details_out1);

  // Check that inserting a duplicate row fails.
  EXPECT_FALSE(databases_table.InsertDatabaseDetails(details_in1));

  // Insert details for another database with the same origin.
  DatabaseDetails details_in2;
  details_in2.origin_identifier = ASCIIToUTF16("origin1");
  details_in2.database_name = ASCIIToUTF16("db2");
  details_in2.description = ASCIIToUTF16("description_db2");
  details_in2.estimated_size = 200;
  EXPECT_TRUE(databases_table.InsertDatabaseDetails(details_in2));
  EXPECT_EQ(2, databases_table.GetDatabaseID(details_in2.origin_identifier,
                                             details_in2.database_name));

  // Insert details for a third database with a different origin.
  DatabaseDetails details_in3;
  details_in3.origin_identifier = ASCIIToUTF16("origin2");
  details_in3.database_name = ASCIIToUTF16("db3");
  details_in3.description = ASCIIToUTF16("description_db3");
  details_in3.estimated_size = 300;
  EXPECT_TRUE(databases_table.InsertDatabaseDetails(details_in3));
  EXPECT_EQ(3, databases_table.GetDatabaseID(details_in3.origin_identifier,
                                             details_in3.database_name));

  // There should be no database with origin "origin3".
  std::vector<DatabaseDetails> details_out_origin3;
  EXPECT_TRUE(databases_table.GetAllDatabaseDetailsForOrigin(
      ASCIIToUTF16("origin3"), &details_out_origin3));
  EXPECT_TRUE(details_out_origin3.empty());

  // There should be only two databases with origin "origin1".
  std::vector<DatabaseDetails> details_out_origin1;
  EXPECT_TRUE(databases_table.GetAllDatabaseDetailsForOrigin(
      details_in1.origin_identifier, &details_out_origin1));
  EXPECT_EQ(size_t(2), details_out_origin1.size());
  CheckDetailsAreEqual(details_in1, details_out_origin1[0]);
  CheckDetailsAreEqual(details_in2, details_out_origin1[1]);

  // Delete the details for 'db1' and check that they're no longer there.
  EXPECT_TRUE(databases_table.DeleteDatabaseDetails(
      details_in1.origin_identifier, details_in1.database_name));
  EXPECT_FALSE(databases_table.GetDatabaseDetails(
      details_in1.origin_identifier,
      details_in1.database_name,
      &details_out1));

  // Check that trying to delete a record that doesn't exist fails.
  EXPECT_FALSE(databases_table.DeleteDatabaseDetails(
      ASCIIToUTF16("unknown_origin"), ASCIIToUTF16("unknown_database")));
}

}  // namespace webkit_database
