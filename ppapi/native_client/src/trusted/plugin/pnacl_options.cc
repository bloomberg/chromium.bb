// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/pnacl_options.h"

#include <iterator>
#include <vector>

#include "native_client/src/include/nacl_string.h"

namespace plugin {

PnaclOptions::PnaclOptions() : translate_(false), opt_level_(2) { }

PnaclOptions::~PnaclOptions() {
}

void PnaclOptions::set_opt_level(int32_t l) {
  if (l <= 0) {
    opt_level_ = 0;
    return;
  }
  // Currently only allow 0 or 2, since that is what we test.
  opt_level_ = 2;
}

std::vector<char> PnaclOptions::GetOptCommandline() const {
  std::vector<char> result;
  nacl::string str;

  nacl::stringstream ss;
  ss << "-O" << opt_level_;
  str = ss.str();

  std::copy(str.begin(), str.end(), std::back_inserter(result));
  result.push_back('\x00');

  return result;
}

}  // namespace plugin
