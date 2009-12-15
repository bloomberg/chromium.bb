// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_SHELL_DEVTOOLS_CALLARGS_H_
#define TEST_SHELL_DEVTOOLS_CALLARGS_H_

#include "base/logging.h"

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

class TestShellDevToolsCallArgs {
 public:
  TestShellDevToolsCallArgs(const WebKit::WebDevToolsMessageData& data)
      : data_(data) {
    ++calls_count_;

    // The same behaiviour as we have in case of IPC.
    for (size_t i = 0; i < data_.arguments.size(); ++i)
      if (data_.arguments[i].isNull())
        data_.arguments[i] = WebKit::WebString::fromUTF8("");
  }

  TestShellDevToolsCallArgs(const TestShellDevToolsCallArgs& args)
      : data_(args.data_) {
    ++calls_count_;
  }

  ~TestShellDevToolsCallArgs() {
    --calls_count_;
    DCHECK(calls_count_ >= 0);
  }

  static int calls_count() { return calls_count_; }

  WebKit::WebDevToolsMessageData data_;

 private:
  static int calls_count_;
};

#endif  // TEST_SHELL_DEVTOOLS_CALLARGS_H_
