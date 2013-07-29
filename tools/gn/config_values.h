// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_CONFIG_VALUES_H_
#define TOOLS_GN_CONFIG_VALUES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "tools/gn/source_dir.h"

// Holds the values (includes, defines, compiler flags, etc.) for a given
// config or target.
class ConfigValues {
 public:
  ConfigValues();
  ~ConfigValues();

  const std::vector<SourceDir>& includes() const { return includes_; }
  void swap_in_includes(std::vector<SourceDir>* lo) { includes_.swap(*lo); }

  const std::vector<std::string>& defines() const { return defines_; }
  void swap_in_defines(std::vector<std::string>* d) { defines_.swap(*d); }

  const std::vector<std::string>& cflags() const { return cflags_; }
  void swap_in_cflags(std::vector<std::string>* lo) { cflags_.swap(*lo); }

  const std::vector<std::string>& cflags_c() const { return cflags_c_; }
  void swap_in_cflags_c(std::vector<std::string>* lo) { cflags_c_.swap(*lo); }

  const std::vector<std::string>& cflags_cc() const { return cflags_cc_; }
  void swap_in_cflags_cc(std::vector<std::string>* lo) { cflags_cc_.swap(*lo); }

  const std::vector<std::string>& ldflags() const { return ldflags_; }
  void swap_in_ldflags(std::vector<std::string>* lo) { ldflags_.swap(*lo); }

 private:
  std::vector<SourceDir> includes_;
  std::vector<std::string> defines_;
  std::vector<std::string> cflags_;
  std::vector<std::string> cflags_c_;
  std::vector<std::string> cflags_cc_;
  std::vector<std::string> ldflags_;

  DISALLOW_COPY_AND_ASSIGN(ConfigValues);
};

#endif  // TOOLS_GN_CONFIG_VALUES_H_
