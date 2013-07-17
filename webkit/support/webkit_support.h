// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBKIT_SUPPORT_H_
#define WEBKIT_SUPPORT_WEBKIT_SUPPORT_H_

// This package provides functions used by DumpRenderTree/chromium.
namespace webkit_support {

// Initializes or terminates a test environment.
// |unit_test_mode| should be set to true when running in a TestSuite, in which
// case no AtExitManager is created and ICU is not initialized (as it is already
// done by the TestSuite).
// SetUpTestEnvironment() and SetUpTestEnvironmentForUnitTests() calls
// WebKit::initialize().
// TearDownTestEnvironment() calls WebKit::shutdown().
// SetUpTestEnvironmentForUnitTests() should be used when running in a
// TestSuite, in which case no AtExitManager is created and ICU is not
// initialized (as it is already done by the TestSuite).
void SetUpTestEnvironmentForUnitTests();
void TearDownTestEnvironment();

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_WEBKIT_SUPPORT_H_
