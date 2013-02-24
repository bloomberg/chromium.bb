// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_DATABASE_H_

#include <map>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "sql/connection.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace dom_storage {

// Represents a SQLite based backing for DOM storage data. This
// class is designed to be used on a single thread.
class WEBKIT_STORAGE_EXPORT DomStorageDatabase {
 public:
  static base::FilePath GetJournalFilePath(const base::FilePath& database_path);

  explicit DomStorageDatabase(const base::FilePath& file_path);
  virtual ~DomStorageDatabase();  // virtual for unit testing

  // Reads all the key, value pairs stored in the database and returns
  // them. |result| is assumed to be empty and any duplicate keys will
  // be overwritten. If the database exists on disk then it will be
  // opened. If it does not exist then it will not be created and
  // |result| will be unmodified.
  void ReadAllValues(ValuesMap* result);

  // Updates the backing database. Will remove all keys before updating
  // the database if |clear_all_first| is set. Then all entries in
  // |changes| will be examined - keys mapped to a null NullableString16
  // will be removed and all others will be inserted/updated as appropriate.
  bool CommitChanges(bool clear_all_first, const ValuesMap& changes);

  // Simple getter for the path we were constructed with.
  const base::FilePath& file_path() const { return file_path_; }

 protected:
  // Constructor that uses an in-memory sqlite database, for testing.
  DomStorageDatabase();

 private:
  friend class LocalStorageDatabaseAdapter;
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest, SimpleOpenAndClose);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest, TestLazyOpenIsLazy);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest, TestDetectSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest,
                           TestLazyOpenUpgradesDatabase);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest, SimpleWriteAndReadBack);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest, WriteWithClear);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest,
                           UpgradeFromV1ToV2WithData);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest, TestSimpleRemoveOneValue);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest,
                           TestCanOpenAndReadWebCoreDatabase);
  FRIEND_TEST_ALL_PREFIXES(DomStorageDatabaseTest,
                           TestCanOpenFileThatIsNotADatabase);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, PurgeMemory);

  enum SchemaVersion {
    INVALID,
    V1,
    V2
  };

  // Open the database at file_path_ if it exists already and creates it if
  // |create_if_needed| is true.
  // Ensures we are at the correct database version and creates or updates
  // tables as necessary. Returns false on failure.
  bool LazyOpen(bool create_if_needed);

  // Analyses the database to verify that the connection that is open is indeed
  // a valid database and works out the schema version.
  SchemaVersion DetectSchemaVersion();

  // Creates the database table at V2. Returns true if the table was created
  // successfully, false otherwise. Will return false if the table already
  // exists.
  bool CreateTableV2();

  // If we have issues while trying to open the file (corrupted databse,
  // failing to upgrade, that sort of thing) this function will remove
  // the file from disk and attempt to create a new database from
  // scratch.
  bool DeleteFileAndRecreate();

  // Version 1 -> 2 migrates the value column in the ItemTable from a TEXT
  // to a BLOB. Exisitng data is preserved on success. Returns false if the
  // upgrade failed. If true is returned, the database is guaranteed to be at
  // version 2.
  bool UpgradeVersion1To2();

  void Close();
  bool IsOpen() const { return db_.get() ? db_->is_open() : false; }

  // Initialization code shared between the two constructors of this class.
  void Init();

  // Path to the database on disk.
  const base::FilePath file_path_;
  scoped_ptr<sql::Connection> db_;
  bool failed_to_open_;
  bool tried_to_recreate_;
  bool known_to_be_empty_;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
