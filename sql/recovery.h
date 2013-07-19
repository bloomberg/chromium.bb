// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVERY_H_
#define SQL_RECOVERY_H_

#include "base/basictypes.h"

#include "sql/connection.h"

namespace base {
class FilePath;
}

namespace sql {

// Recovery module for sql/.  The basic idea is to create a fresh
// database and populate it with the recovered contents of the
// original database.  If recovery is successful, the recovered
// database is backed up over the original database.  If recovery is
// not successful, the original database is razed.  In either case,
// the original handle is poisoned so that operations on the stack do
// not accidentally disrupt the restored data.
//
// {
//   scoped_ptr<sql::Recovery> r =
//       sql::Recovery::Begin(orig_db, orig_db_path);
//   if (r) {
//     if (r.db()->Execute(kCreateSchemaSql) &&
//         r.db()->Execute(kCopyDataFromOrigSql)) {
//       sql::Recovery::Recovered(r.Pass());
//     }
//   }
// }
//
// If Recovered() is not called, then RazeAndClose() is called on
// orig_db.

class SQL_EXPORT Recovery {
 public:
  ~Recovery();

  // Begin the recovery process by opening a temporary database handle
  // and attach the existing database to it at "corrupt".  To prevent
  // deadlock, all transactions on |connection| are rolled back.
  //
  // Returns NULL in case of failure, with no cleanup done on the
  // original connection (except for breaking the transactions).  The
  // caller should Raze() or otherwise cleanup as appropriate.
  //
  // TODO(shess): Later versions of SQLite allow extracting the path
  // from the connection.
  // TODO(shess): Allow specifying the connection point?
  static scoped_ptr<Recovery> Begin(
      Connection* connection,
      const base::FilePath& db_path) WARN_UNUSED_RESULT;

  // Mark recovery completed by replicating the recovery database over
  // the original database, then closing the recovery database.  The
  // original database handle is poisoned, causing future calls
  // against it to fail.
  //
  // If Recovered() is not called, the destructor will call
  // Unrecoverable().
  //
  // TODO(shess): At this time, this function an fail while leaving
  // the original database intact.  Figure out which failure cases
  // should go to RazeAndClose() instead.
  static bool Recovered(scoped_ptr<Recovery> r) WARN_UNUSED_RESULT;

  // Indicate that the database is unrecoverable.  The original
  // database is razed, and the handle poisoned.
  static void Unrecoverable(scoped_ptr<Recovery> r);

  // Handle to the temporary recovery database.
  sql::Connection* db() { return &recover_db_; }

 private:
  explicit Recovery(Connection* connection);

  // Setup the recovery database handle for Begin().  Returns false in
  // case anything failed.
  bool Init(const base::FilePath& db_path) WARN_UNUSED_RESULT;

  // Copy the recovered database over the original database.
  bool Backup() WARN_UNUSED_RESULT;

  // Close the recovery database, and poison the original handle.
  // |raze| controls whether the original database is razed or just
  // poisoned.
  enum Disposition {
    RAZE_AND_POISON,
    POISON,
  };
  void Shutdown(Disposition raze);

  Connection* db_;         // Original database connection.
  Connection recover_db_;  // Recovery connection.

  DISALLOW_COPY_AND_ASSIGN(Recovery);
};

}  // namespace sql

#endif  // SQL_RECOVERY_H_
