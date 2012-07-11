// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_INCLUDES_TEST_UNRECOVERABLE_ERROR_HANDLER_H_
#define SYNC_INTERNAL_API_INCLUDES_TEST_UNRECOVERABLE_ERROR_HANDLER_H_

#include "base/compiler_specific.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"

namespace syncer {

// Implementation of UnrecoverableErrorHandler that simply adds a
// gtest failure.
class TestUnrecoverableErrorHandler : public UnrecoverableErrorHandler {
 public:
  TestUnrecoverableErrorHandler();
  virtual ~TestUnrecoverableErrorHandler();

  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_INCLUDES_TEST_UNRECOVERABLE_ERROR_HANDLER_H_

