// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_EDGE_DATABASE_READER_WIN_H_
#define CHROME_UTILITY_IMPORTER_EDGE_DATABASE_READER_WIN_H_

#define JET_UNICODE
#include <esent.h>
#undef JET_UNICODE

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"

class EdgeErrorObject {
 public:
  EdgeErrorObject() : last_error_(JET_errSuccess) {}

  // Get the last error converted to a descriptive string.
  base::string16 GetErrorMessage() const;
  // Get the last error value.
  JET_ERR last_error() const { return last_error_; }

 protected:
  // This function returns true if the passed error parameter is equal
  // to JET_errSuccess
  bool SetLastError(JET_ERR error);

 private:
  JET_ERR last_error_;

  DISALLOW_COPY_AND_ASSIGN(EdgeErrorObject);
};

class EdgeDatabaseTableEnumerator : public EdgeErrorObject {
 public:
  EdgeDatabaseTableEnumerator(const base::string16& table_name,
                              JET_SESID session_id,
                              JET_TABLEID table_id);

  ~EdgeDatabaseTableEnumerator();

  const base::string16& table_name() { return table_name_; }

  // Reset the enumerator to the start of the table. Returns true if successful.
  bool Reset();
  // Move to the next row in the table. Returns false on error or no more rows.
  bool Next();

  // Retrieve a column's data value. If a NULL is encountered in the column the
  // default value for the template type is placed in |value|.
  template <typename T>
  bool RetrieveColumn(const base::string16& column_name, T* value);

 private:
  const JET_COLUMNBASE& GetColumnByName(const base::string16& column_name);

  std::map<const base::string16, JET_COLUMNBASE> columns_by_name_;
  JET_TABLEID table_id_;
  base::string16 table_name_;
  JET_SESID session_id_;

  DISALLOW_COPY_AND_ASSIGN(EdgeDatabaseTableEnumerator);
};

class EdgeDatabaseReader : public EdgeErrorObject {
 public:
  EdgeDatabaseReader()
      : db_id_(JET_dbidNil),
        instance_id_(JET_instanceNil),
        session_id_(JET_sesidNil) {}

  ~EdgeDatabaseReader();

  // Open the database from a file path. Returns true on success.
  bool OpenDatabase(const base::string16& database_file);

  // Open a row enumerator for a specified table. Returns a nullptr on error.
  std::unique_ptr<EdgeDatabaseTableEnumerator> OpenTableEnumerator(
      const base::string16& table_name);

 private:
  bool IsOpen() { return instance_id_ != JET_instanceNil; }

  JET_DBID db_id_;
  JET_INSTANCE instance_id_;
  JET_SESID session_id_;

  DISALLOW_COPY_AND_ASSIGN(EdgeDatabaseReader);
};

#endif  // CHROME_UTILITY_IMPORTER_EDGE_DATABASE_READER_WIN_H_
