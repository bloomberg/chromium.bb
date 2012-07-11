// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_REPORT_UNRECOVERABLE_ERROR_FUNCTION_H_
#define SYNC_UTIL_REPORT_UNRECOVERABLE_ERROR_FUNCTION_H_

namespace syncer {

// A ReportUnrecoverableErrorFunction is a function that is called
// immediately when an unrecoverable error is encountered.  Unlike
// UnrecoverableErrorHandler, it should just log the error and any
// context surrounding it.
typedef void (*ReportUnrecoverableErrorFunction)(void);

}  // namespace syncer

#endif  // SYNC_UTIL_REPORT_UNRECOVERABLE_ERROR_FUNCTION_H_
