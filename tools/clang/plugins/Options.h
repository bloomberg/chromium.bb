// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_OPTIONS_H_
#define TOOLS_CLANG_PLUGINS_OPTIONS_H_

namespace chrome_checker {

struct Options {
  Options()
      : check_base_classes(false),
        check_weak_ptr_factory_order(false),
        check_enum_last_value(false),
        strict_virtual_specifiers(false) {}

  bool check_base_classes;
  bool check_weak_ptr_factory_order;
  bool check_enum_last_value;
  bool strict_virtual_specifiers;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_OPTIONS_H_
