// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/test_with_scope.h"

TestWithScope::TestWithScope()
    : build_settings_(),
      settings_(&build_settings_, std::string()),
      toolchain_(&settings_, Label(SourceDir("//toolchain/"), "default")),
      scope_(&settings_) {
  build_settings_.SetBuildDir(SourceDir("//out/Debug/"));

  settings_.set_toolchain_label(toolchain_.label());
  settings_.set_default_toolchain_label(toolchain_.label());
}

TestWithScope::~TestWithScope() {
}
