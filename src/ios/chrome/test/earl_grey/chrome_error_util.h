// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_ERROR_UTIL_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_ERROR_UTIL_H_

@class NSError;
@class NSString;

namespace chrome_test_util {

// Returns a NSError with generic domain and error code, and the provided string
// as localizedDescription.
NSError* NSErrorWithLocalizedDescription(NSString* error_description);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_ERROR_UTIL_H_
