// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TEST_WITH_SCOPE_H_
#define TOOLS_GN_TEST_WITH_SCOPE_H_

#include "base/basictypes.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/toolchain.h"

// A helper class for setting up a Scope that a test can use. It makes a
// toolchain and sets up all the build state.
class TestWithScope {
 public:
  TestWithScope();
  ~TestWithScope();

  BuildSettings* build_settings() { return &build_settings_; }
  Settings* settings() { return &settings_; }
  Toolchain* toolchain() { return &toolchain_; }
  Scope* scope() { return &scope_; }

 private:
  BuildSettings build_settings_;
  Settings settings_;
  Toolchain toolchain_;
  Scope scope_;

  DISALLOW_COPY_AND_ASSIGN(TestWithScope);
};

#endif  // TOOLS_GN_TEST_WITH_SCOPE_H_
