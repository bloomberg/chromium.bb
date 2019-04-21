// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_META_TABLE_H_
#define SQL_META_TABLE_H_

#include <stdint.h>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"

namespace sql {

class Database;
class Statement;

// Creates and manages a table to store generic metadata. The features provided
// are:
// * An interface for storing multi-typed key-value data.
// * Helper methods to assist in database schema version control.
// * Historical data on past attempts to mmap the database to make it possible
//   to avoid unconditionally retrying to load broken databases.
class COMPONENT_EXPORT(SQL) MetaTable {
 public:
  MetaTable();
  ~MetaTable();

  // Values for Get/SetMmapStatus(). |kMmapFailure| indicates that there was at
  // some point a read error and the database should not be memory-mapped, while
  // |kMmapSuccess| indicates that the entire file was read at some point and
  // can be memory-mapped without constraint.
  static constexpr int64_t kMmapFailure = -2;
  static constexpr int64_t kMmapSuccess = -1;

  // Returns true if the 'meta' table exists.
  static bool DoesTableExist(Database* db);

  // If the current version of the database is less than or equal to
  // |deprecated_version|, raze the database. Must be called outside of a
  // transaction.
  // TODO(shess): At this time the database is razed IFF meta exists and
  // contains a version row with value <= deprecated_version. It may make sense
  // to also raze if meta exists but has no version row, or if meta doesn't
  // exist. In those cases if the database is not already empty, it probably
  // resulted from a broken initialization.
  // TODO(shess): Folding this into Init() would allow enforcing
  // |deprecated_version|<|version|. But Init() is often called in a
  // transaction.
  static void RazeIfDeprecated(Database* db, int deprecated_version);

  // Used to tuck some data into the meta table about mmap status. The value
  // represents how much data in bytes has successfully been read from the
  // database, or |kMmapFailure| or |kMmapSuccess|.
  static bool GetMmapStatus(Database* db, int64_t* status);
  static bool SetMmapStatus(Database* db, int64_t status);

  // Initializes the MetaTableHelper, providing the |Database| pointer and
  // creating the meta table if necessary. Must be called before any other
  // non-static methods. For new tables, it will initialize the version number
  // to |version| and the compatible version number to |compatible_version|.
  // Versions must be greater than 0 to distinguish missing versions (see
  // GetVersionNumber()). If there was no meta table (proxy for a fresh
  // database), mmap status is set to |kMmapSuccess|.
  bool Init(Database* db, int version, int compatible_version);

  // Resets this MetaTable object, making another call to Init() possible.
  void Reset();

  // The version number of the database. This should be the version number of
  // the creator of the file. The version number will be 0 if there is no
  // previously set version number.
  //
  // See also Get/SetCompatibleVersionNumber().
  void SetVersionNumber(int version);
  int GetVersionNumber();

  // The compatible version number is the lowest version of the code that this
  // database can be read by. If there are minor changes or additions, old
  // versions of the code can still work with the database without failing.
  //
  // For example, if an optional column is added to a table in version 3, the
  // new code will set the version to 3, and the compatible version to 2, since
  // the code expecting version 2 databases can still read and write the table.
  //
  // Rule of thumb: check the version number when you're upgrading, but check
  // the compatible version number to see if you can read the file at all. If
  // it's larger than you code is expecting, fail.
  //
  // The compatible version number will be 0 if there is no previously set
  // compatible version number.
  void SetCompatibleVersionNumber(int version);
  int GetCompatibleVersionNumber();

  // Set the given arbitrary key with the given data. Returns true on success.
  bool SetValue(const char* key, const std::string& value);
  bool SetValue(const char* key, int value);
  bool SetValue(const char* key, int64_t value);

  // Retrieves the value associated with the given key. This will use sqlite's
  // type conversion rules. It will return true on success.
  bool GetValue(const char* key, std::string* value);
  bool GetValue(const char* key, int* value);
  bool GetValue(const char* key, int64_t* value);

  // Deletes the key from the table.
  bool DeleteKey(const char* key);

 private:
  // Conveniences to prepare the two types of statements used by
  // MetaTableHelper.
  void PrepareSetStatement(Statement* statement, const char* key);
  bool PrepareGetStatement(Statement* statement, const char* key);

  Database* db_;

  DISALLOW_COPY_AND_ASSIGN(MetaTable);
};

}  // namespace sql

#endif  // SQL_META_TABLE_H_
