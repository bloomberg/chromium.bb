// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recovery.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "sql/connection.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// static
scoped_ptr<Recovery> Recovery::Begin(
    Connection* connection,
    const base::FilePath& db_path) {
  scoped_ptr<Recovery> r(new Recovery(connection));
  if (!r->Init(db_path)) {
    // TODO(shess): Should Init() failure result in Raze()?
    r->Shutdown(POISON);
    return scoped_ptr<Recovery>();
  }

  return r.Pass();
}

// static
bool Recovery::Recovered(scoped_ptr<Recovery> r) {
  return r->Backup();
}

// static
void Recovery::Unrecoverable(scoped_ptr<Recovery> r) {
  CHECK(r->db_);
  // ~Recovery() will RAZE_AND_POISON.
}

Recovery::Recovery(Connection* connection)
    : db_(connection),
      recover_db_() {
  // Result should keep the page size specified earlier.
  if (db_->page_size_)
    recover_db_.set_page_size(db_->page_size_);

  // TODO(shess): This may not handle cases where the default page
  // size is used, but the default has changed.  I do not think this
  // has ever happened.  This could be handled by using "PRAGMA
  // page_size", at the cost of potential additional failure cases.
}

Recovery::~Recovery() {
  Shutdown(RAZE_AND_POISON);
}

bool Recovery::Init(const base::FilePath& db_path) {
  // Prevent the possibility of re-entering this code due to errors
  // which happen while executing this code.
  DCHECK(!db_->has_error_callback());

  // Break any outstanding transactions on the original database to
  // prevent deadlocks reading through the attached version.
  // TODO(shess): A client may legitimately wish to recover from
  // within the transaction context, because it would potentially
  // preserve any in-flight changes.  Unfortunately, any attach-based
  // system could not handle that.  A system which manually queried
  // one database and stored to the other possibly could, but would be
  // more complicated.
  db_->RollbackAllTransactions();

  if (!recover_db_.OpenTemporary())
    return false;

  // TODO(shess): Figure out a story for USE_SYSTEM_SQLITE.  The
  // virtual table implementation relies on SQLite internals for some
  // types and functions, which could be copied inline to make it
  // standalone.  Or an alternate implementation could try to read
  // through errors entirely at the SQLite level.
  //
  // For now, defer to the caller.  The setup will succeed, but the
  // later CREATE VIRTUAL TABLE call will fail, at which point the
  // caller can fire Unrecoverable().
#if !defined(USE_SYSTEM_SQLITE)
  int rc = recoverVtableInit(recover_db_.db_);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to initialize recover module: "
               << recover_db_.GetErrorMessage();
    return false;
  }
#endif

  // Turn on |SQLITE_RecoveryMode| for the handle, which allows
  // reading certain broken databases.
  if (!recover_db_.Execute("PRAGMA writable_schema=1"))
    return false;

  if (!recover_db_.AttachDatabase(db_path, "corrupt"))
    return false;

  return true;
}

bool Recovery::Backup() {
  CHECK(db_);
  CHECK(recover_db_.is_open());

  // TODO(shess): Some of the failure cases here may need further
  // exploration.  Just as elsewhere, persistent problems probably
  // need to be razed, while anything which might succeed on a future
  // run probably should be allowed to try.  But since Raze() uses the
  // same approach, even that wouldn't work when this code fails.
  //
  // The documentation for the backup system indicate a relatively
  // small number of errors are expected:
  // SQLITE_BUSY - cannot lock the destination database.  This should
  //               only happen if someone has another handle to the
  //               database, Chromium generally doesn't do that.
  // SQLITE_LOCKED - someone locked the source database.  Should be
  //                 impossible (perhaps anti-virus could?).
  // SQLITE_READONLY - destination is read-only.
  // SQLITE_IOERR - since source database is temporary, probably
  //                indicates that the destination contains blocks
  //                throwing errors, or gross filesystem errors.
  // SQLITE_NOMEM - out of memory, should be transient.
  //
  // AFAICT, SQLITE_BUSY and SQLITE_NOMEM could perhaps be considered
  // transient, with SQLITE_LOCKED being unclear.
  //
  // SQLITE_READONLY and SQLITE_IOERR are probably persistent, with a
  // strong chance that Raze() would not resolve them.  If Delete()
  // deletes the database file, the code could then re-open the file
  // and attempt the backup again.
  //
  // For now, this code attempts a best effort and records histograms
  // to inform future development.

  // Backup the original db from the recovered db.
  const char* kMain = "main";
  sqlite3_backup* backup = sqlite3_backup_init(db_->db_, kMain,
                                               recover_db_.db_, kMain);
  if (!backup) {
    // Error code is in the destination database handle.
    int err = sqlite3_errcode(db_->db_);
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RecoveryHandle", err);
    LOG(ERROR) << "sqlite3_backup_init() failed: "
               << sqlite3_errmsg(db_->db_);
    return false;
  }

  // -1 backs up the entire database.
  int rc = sqlite3_backup_step(backup, -1);
  int pages = sqlite3_backup_pagecount(backup);
  // TODO(shess): sqlite3_backup_finish() appears to allow returning a
  // different value from sqlite3_backup_step().  Circle back and
  // figure out if that can usefully inform the decision of whether to
  // retry or not.
  sqlite3_backup_finish(backup);
  DCHECK_GT(pages, 0);

  if (rc != SQLITE_DONE) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RecoveryStep", rc);
    LOG(ERROR) << "sqlite3_backup_step() failed: "
               << sqlite3_errmsg(db_->db_);
  }

  // The destination database was locked.  Give up, but leave the data
  // in place.  Maybe it won't be locked next time.
  if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
    Shutdown(POISON);
    return false;
  }

  // Running out of memory should be transient, retry later.
  if (rc == SQLITE_NOMEM) {
    Shutdown(POISON);
    return false;
  }

  // TODO(shess): For now, leave the original database alone, pending
  // results from Sqlite.RecoveryStep.  Some errors should probably
  // route to RAZE_AND_POISON.
  if (rc != SQLITE_DONE) {
    Shutdown(POISON);
    return false;
  }

  // Clean up the recovery db, and terminate the main database
  // connection.
  Shutdown(POISON);
  return true;
}

void Recovery::Shutdown(Recovery::Disposition raze) {
  if (!db_)
    return;

  recover_db_.Close();
  if (raze == RAZE_AND_POISON) {
    db_->RazeAndClose();
  } else if (raze == POISON) {
    db_->Poison();
  }
  db_ = NULL;
}

}  // namespace sql
