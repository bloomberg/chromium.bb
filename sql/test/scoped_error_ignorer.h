// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_SCOPED_ERROR_IGNORER_H_
#define SQL_TEST_SCOPED_ERROR_IGNORER_H_

#include <set>

#include "base/basictypes.h"
#include "sql/connection.h"

namespace sql {

// sql::Connection and sql::Statement treat most SQLite errors as
// fatal in debug mode.  The intention is to catch inappropriate uses
// of SQL before the code is shipped to production.  This makes it
// challenging to write tests for things like recovery from
// corruption.  This scoper can be used to ignore selected errors
// during a test.  Errors are ignored globally (on all connections).
//
// Since errors can be very context-dependent, the class is pedantic -
// specific errors must be ignored, and every error ignored must be
// seen.
//
// NOTE(shess): There are still fatal error cases this does not
// address.  If your test is handling database errors and you're
// hitting a case not handled, contact me.
class ScopedErrorIgnorer {
 public:
  ScopedErrorIgnorer();
  ~ScopedErrorIgnorer();

  // Add an error to ignore.  Extended error codes can be ignored
  // specifically, or the base code can ignore an entire group
  // (SQLITE_IOERR_* versus SQLITE_IOERR).
  void IgnoreError(int err);

  // Allow containing test to check if the errors were encountered.
  // Failure to call results in ADD_FAILURE() in destructor.
  bool CheckIgnoredErrors();

  // Record an error and check if it should be ignored.
  bool ShouldIgnore(int err);

 private:
  // Storage for callback passed to Connection::SetErrorIgnorer().
  Connection::ErrorIgnorerCallback callback_;

  // Record whether CheckIgnoredErrors() has been called.
  bool checked_;

  // Errors to ignore.
  std::set<int> ignore_errors_;

  // Errors which have been ignored.
  std::set<int> errors_ignored_;

  DISALLOW_COPY_AND_ASSIGN(ScopedErrorIgnorer);
};

}  // namespace sql

#endif  // SQL_TEST_SCOPED_ERROR_IGNORER_H_
