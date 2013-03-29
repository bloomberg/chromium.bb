// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "sql/diagnostic_error_delegate.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

const base::FilePath::CharType kJournal[] = FILE_PATH_LITERAL("-journal");

class HistogramUniquifier {
 public:
  static const char* name() { return "Sqlite.DomStorageDatabase.Error"; }
};

sql::ErrorDelegate* GetErrorHandlerForDomStorageDatabase() {
  return new sql::DiagnosticErrorDelegate<HistogramUniquifier>();
}

}  // anon namespace

namespace dom_storage {

// static
base::FilePath DomStorageDatabase::GetJournalFilePath(
    const base::FilePath& database_path) {
  base::FilePath::StringType journal_file_name =
      database_path.BaseName().value() + kJournal;
  return database_path.DirName().Append(journal_file_name);
}

DomStorageDatabase::DomStorageDatabase(const base::FilePath& file_path)
    : file_path_(file_path) {
  // Note: in normal use we should never get an empty backing path here.
  // However, the unit test for this class can contruct an instance
  // with an empty path.
  Init();
}

DomStorageDatabase::DomStorageDatabase() {
  Init();
}

void DomStorageDatabase::Init() {
  failed_to_open_ = false;
  tried_to_recreate_ = false;
  known_to_be_empty_ = false;
}

DomStorageDatabase::~DomStorageDatabase() {
  if (known_to_be_empty_ && !file_path_.empty()) {
    // Delete the db and any lingering journal file from disk.
    Close();
    file_util::Delete(file_path_, false);
    file_util::Delete(GetJournalFilePath(file_path_), false);
  }
}

void DomStorageDatabase::ReadAllValues(ValuesMap* result) {
  if (!LazyOpen(false))
    return;

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                   "SELECT * from ItemTable"));
  DCHECK(statement.is_valid());

  while (statement.Step()) {
    base::string16 key = statement.ColumnString16(0);
    base::string16 value;
    statement.ColumnBlobAsString16(1, &value);
    (*result)[key] = NullableString16(value, false);
  }
  known_to_be_empty_ = result->empty();
}

bool DomStorageDatabase::CommitChanges(bool clear_all_first,
                                       const ValuesMap& changes) {
  if (!LazyOpen(!changes.empty())) {
    // If we're being asked to commit changes that will result in an
    // empty database, we return true if the database file doesn't exist.
    return clear_all_first && changes.empty() &&
           !file_util::PathExists(file_path_);
  }

  bool old_known_to_be_empty = known_to_be_empty_;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (clear_all_first) {
    if (!db_->Execute("DELETE FROM ItemTable"))
      return false;
    known_to_be_empty_ = true;
  }

  bool did_delete = false;
  bool did_insert = false;
  ValuesMap::const_iterator it = changes.begin();
  for(; it != changes.end(); ++it) {
    sql::Statement statement;
    base::string16 key = it->first;
    NullableString16 value = it->second;
    if (value.is_null()) {
      statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE,
         "DELETE FROM ItemTable WHERE key=?"));
      statement.BindString16(0, key);
      did_delete = true;
    } else {
      statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE,
          "INSERT INTO ItemTable VALUES (?,?)"));
      statement.BindString16(0, key);
      statement.BindBlob(1, value.string().data(),
                         value.string().length() * sizeof(char16));
      known_to_be_empty_ = false;
      did_insert = true;
    }
    DCHECK(statement.is_valid());
    statement.Run();
  }

  if (!known_to_be_empty_ && did_delete && !did_insert) {
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
        "SELECT count(key) from ItemTable"));
    if (statement.Step())
      known_to_be_empty_ = statement.ColumnInt(0) == 0;
  }

  bool success = transaction.Commit();
  if (!success)
    known_to_be_empty_ = old_known_to_be_empty;
  return success;
}

bool DomStorageDatabase::LazyOpen(bool create_if_needed) {
  if (failed_to_open_) {
    // Don't try to open a database that we know has failed
    // already.
    return false;
  }

  if (IsOpen())
    return true;

  bool database_exists = file_util::PathExists(file_path_);

  if (!database_exists && !create_if_needed) {
    // If the file doesn't exist already and we haven't been asked to create
    // a file on disk, then we don't bother opening the database. This means
    // we wait until we absolutely need to put something onto disk before we
    // do so.
    return false;
  }

  db_.reset(new sql::Connection());
  db_->set_error_delegate(GetErrorHandlerForDomStorageDatabase());

  if (file_path_.empty()) {
    // This code path should only be triggered by unit tests.
    if (!db_->OpenInMemory()) {
      NOTREACHED() << "Unable to open DOM storage database in memory.";
      failed_to_open_ = true;
      return false;
    }
  } else {
    if (!db_->Open(file_path_)) {
      LOG(ERROR) << "Unable to open DOM storage database at "
                 << file_path_.value()
                 << " error: " << db_->GetErrorMessage();
      if (database_exists && !tried_to_recreate_)
        return DeleteFileAndRecreate();
      failed_to_open_ = true;
      return false;
    }
  }

  // sql::Connection uses UTF-8 encoding, but WebCore style databases use
  // UTF-16, so ensure we match.
  ignore_result(db_->Execute("PRAGMA encoding=\"UTF-16\""));

  if (!database_exists) {
    // This is a new database, create the table and we're done!
    if (CreateTableV2())
      return true;
  } else {
    // The database exists already - check if we need to upgrade
    // and whether it's usable (i.e. not corrupted).
    SchemaVersion current_version = DetectSchemaVersion();

    if (current_version == V2) {
      return true;
    } else if (current_version == V1) {
      if (UpgradeVersion1To2())
        return true;
    }
  }

  // This is the exceptional case - to try and recover we'll attempt
  // to delete the file and start again.
  Close();
  return DeleteFileAndRecreate();
}

DomStorageDatabase::SchemaVersion DomStorageDatabase::DetectSchemaVersion() {
  DCHECK(IsOpen());

  // Connection::Open() may succeed even if the file we try and open is not a
  // database, however in the case that the database is corrupted to the point
  // that SQLite doesn't actually think it's a database,
  // sql::Connection::GetCachedStatement will DCHECK when we later try and
  // run statements. So we run a query here that will not DCHECK but fail
  // on an invalid database to verify that what we've opened is usable.
  if (db_->ExecuteAndReturnErrorCode("PRAGMA auto_vacuum") != SQLITE_OK)
    return INVALID;

  // Look at the current schema - if it doesn't look right, assume corrupt.
  if (!db_->DoesTableExist("ItemTable") ||
      !db_->DoesColumnExist("ItemTable", "key") ||
      !db_->DoesColumnExist("ItemTable", "value"))
    return INVALID;

  // We must use a unique statement here as we aren't going to step it.
  sql::Statement statement(
      db_->GetUniqueStatement("SELECT key,value from ItemTable LIMIT 1"));
  if (statement.DeclaredColumnType(0) != sql::COLUMN_TYPE_TEXT)
    return INVALID;

  switch (statement.DeclaredColumnType(1)) {
    case sql::COLUMN_TYPE_BLOB:
      return V2;
    case sql::COLUMN_TYPE_TEXT:
      return V1;
    default:
      return INVALID;
  }
  NOTREACHED();
  return INVALID;
}

bool DomStorageDatabase::CreateTableV2() {
  DCHECK(IsOpen());

  return db_->Execute(
      "CREATE TABLE ItemTable ("
      "key TEXT UNIQUE ON CONFLICT REPLACE, "
      "value BLOB NOT NULL ON CONFLICT FAIL)");
}

bool DomStorageDatabase::DeleteFileAndRecreate() {
  DCHECK(!IsOpen());
  DCHECK(file_util::PathExists(file_path_));

  // We should only try and do this once.
  if (tried_to_recreate_)
    return false;

  tried_to_recreate_ = true;

  // If it's not a directory and we can delete the file, try and open it again.
  if (!file_util::DirectoryExists(file_path_) &&
      file_util::Delete(file_path_, false))
    return LazyOpen(true);

  failed_to_open_ = true;
  return false;
}

bool DomStorageDatabase::UpgradeVersion1To2() {
  DCHECK(IsOpen());
  DCHECK(DetectSchemaVersion() == V1);

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
      "SELECT * FROM ItemTable"));
  DCHECK(statement.is_valid());

  // Need to migrate from TEXT value column to BLOB.
  // Store the current database content so we can re-insert
  // the data into the new V2 table.
  ValuesMap values;
  while (statement.Step()) {
    base::string16 key = statement.ColumnString16(0);
    NullableString16 value(statement.ColumnString16(1), false);
    values[key] = value;
  }

  sql::Transaction migration(db_.get());
  return migration.Begin() &&
      db_->Execute("DROP TABLE ItemTable") &&
      CreateTableV2() &&
      CommitChanges(false, values) &&
      migration.Commit();
}

void DomStorageDatabase::Close() {
  db_.reset(NULL);
}

}  // namespace dom_storage
