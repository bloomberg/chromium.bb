// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_OPTIONS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_OPTIONS_H_

#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"

namespace plugin {

// Options for PNaCl translation.
class PnaclOptions {

 public:
  PnaclOptions();
  ~PnaclOptions();

  // Return a character array of \x00 delimited commandline options.
  std::vector<char> GetOptCommandline() const;

  bool translate() const { return translate_; }
  void set_translate(bool t) { translate_ = t; }

  bool is_debug() const { return is_debug_; }
  void set_debug(bool t) { is_debug_ = t; }

  int32_t opt_level() const { return opt_level_; }
  void set_opt_level(int32_t l);

 private:
  // NOTE: There are users of this class that use the copy constructor.
  // Currently the default copy constructor is good enough, but
  // double-check that it is the case when more fields are added.
  bool translate_;
  bool is_debug_;
  int32_t opt_level_;
};

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_OPTIONS_H_
