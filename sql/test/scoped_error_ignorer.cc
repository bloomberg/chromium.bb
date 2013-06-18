// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/scoped_error_ignorer.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

ScopedErrorIgnorer::ScopedErrorIgnorer()
    : checked_(false) {
  callback_ =
      base::Bind(&ScopedErrorIgnorer::ShouldIgnore, base::Unretained(this));
  Connection::SetErrorIgnorer(&callback_);
}

ScopedErrorIgnorer::~ScopedErrorIgnorer() {
  EXPECT_TRUE(checked_) << " Test must call CheckIgnoredErrors()";
  Connection::ResetErrorIgnorer();
}

void ScopedErrorIgnorer::IgnoreError(int err) {
  EXPECT_EQ(0u, ignore_errors_.count(err))
      << " Error " << err << " is already ignored";
  ignore_errors_.insert(err);
}

bool ScopedErrorIgnorer::CheckIgnoredErrors() {
  checked_ = true;
  return errors_ignored_ == ignore_errors_;
}

bool ScopedErrorIgnorer::ShouldIgnore(int err) {
  // Look for extended code.
  if (ignore_errors_.count(err) > 0) {
    // Record that the error was seen and ignore it.
    errors_ignored_.insert(err);
    return true;
  }

  // Trim extended codes and check again.
  int base_err = err & 0xff;
  if (ignore_errors_.count(base_err) > 0) {
    // Record that the error was seen and ignore it.
    errors_ignored_.insert(base_err);
    return true;
  }

  // Unexpected error.
  ADD_FAILURE() << " Unexpected SQLite error " << err;

  // TODO(shess): If it never makes sense to pass through an error
  // under the test harness, then perhaps the ignore callback
  // signature should be changed.
  return true;
}

}  // namespace sql
