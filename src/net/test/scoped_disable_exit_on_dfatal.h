// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SCOPED_DISABLE_EXIT_ON_DFATAL_H_
#define NET_TEST_SCOPED_DISABLE_EXIT_ON_DFATAL_H_

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

// The ScopedDisableExitOnDFatal class is used to disable exiting the
// program when we encounter a LOG(DFATAL) within the current block.
// After we leave the current block, the default behavior is
// restored.
class ScopedDisableExitOnDFatal {
 public:
  ScopedDisableExitOnDFatal();
  ~ScopedDisableExitOnDFatal();

 private:
  // Static function which is set as the logging assert handler.
  // Called when there is a check failure.
  static void LogAssertHandler(const char* file,
                               int line,
                               const base::StringPiece message,
                               const base::StringPiece stack_trace);

  logging::ScopedLogAssertHandler assert_handler_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisableExitOnDFatal);
};

}  // namespace test
}  // namespace net

#endif  // NET_TEST_SCOPED_DISABLE_EXIT_ON_DFATAL_H_
