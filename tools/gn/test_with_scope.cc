// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/test_with_scope.h"

TestWithScope::TestWithScope()
    : build_settings_(),
      toolchain_(Label(SourceDir("//toolchain/"), "tc")),
      settings_(&build_settings_, &toolchain_, std::string()),
      scope_(&settings_) {
}

TestWithScope::~TestWithScope() {
}
