// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_ERROR_UTIL_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_ERROR_UTIL_H_

// Wraps an expression that returns an NSError*, asserting if an error is
// returned. Used in EG test code to assert if app helpers fail. For example:
//  CHROME_EG_ASSERT_NO_ERROR(helperReturningNSError());
//  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey helperReturningNSError]);
#define CHROME_EG_ASSERT_NO_ERROR(expression)                           \
  {                                                                     \
    NSError* error = expression;                                        \
    GREYAssert(error == nil || [error isKindOfClass:[NSError class]],   \
               @"Expression did not return an object of type NSError"); \
    GREYAssertNil(error, error.localizedDescription);                   \
  }

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_ERROR_UTIL_H_
