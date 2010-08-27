// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_devtools_callargs.h"

#include "base/logging.h"

// static
int TestShellDevToolsCallArgs::calls_count_ = 0;

TestShellDevToolsCallArgs::TestShellDevToolsCallArgs(
    const WebKit::WebString& data)
    : data_(data) {
  ++calls_count_;
}

TestShellDevToolsCallArgs::TestShellDevToolsCallArgs(
    const TestShellDevToolsCallArgs& args)
    : data_(args.data_) {
  ++calls_count_;
}

TestShellDevToolsCallArgs::~TestShellDevToolsCallArgs() {
  --calls_count_;
  DCHECK(calls_count_ >= 0);
}
