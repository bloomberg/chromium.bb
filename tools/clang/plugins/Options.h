// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_OPTIONS_H_
#define TOOLS_CLANG_PLUGINS_OPTIONS_H_

namespace chrome_checker {

struct Options {
  bool check_base_classes = false;
  bool enforce_in_thirdparty_webkit = false;  // Use in Blink code itself
  bool check_enum_max_value = false;
  bool check_ipc = false;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_OPTIONS_H_
