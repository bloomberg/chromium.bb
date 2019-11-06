// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_HISTORY_UNITTEST_BASE_H_
#define COMPONENTS_HISTORY_CORE_TEST_HISTORY_UNITTEST_BASE_H_

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class FilePath;
}

namespace history {
// A base class for a history unit test. It provides the common test methods.
//
class HistoryUnitTestBase : public testing::Test {
 public:
  ~HistoryUnitTestBase() override;

  // Executes the sql from the file |sql_path| in the database at |db_path|.
  // |sql_path| is the SQL script file name with full path.
  // |db_path| is the db file name with full path.
  static void ExecuteSQLScript(const base::FilePath& sql_path,
                               const base::FilePath& db_path);

 protected:
  HistoryUnitTestBase();

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUnitTestBase);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_HISTORY_UNITTEST_BASE_H_
