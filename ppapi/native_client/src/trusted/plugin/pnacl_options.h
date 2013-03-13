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

  // Return true if we know the hash of the bitcode, for caching.
  bool HasCacheKey() { return bitcode_hash_ != ""; }

  // Return the cache key (which takes into account the bitcode hash,
  // as well as the commandline options).
  nacl::string GetCacheKey();

  // Return true if the manifest did not specify any special options
  // (just using the default).
  bool HasDefaultOpts() {
    return opt_level_ == -1 && experimental_flags_ == "";
  }

  // Return a character array of \x00 delimited commandline options.
  std::vector<char> GetOptCommandline();

  bool translate() { return translate_; }
  void set_translate(bool t) { translate_ = t; }

  uint8_t opt_level() { return opt_level_; }
  void set_opt_level(int8_t l);

  nacl::string experimental_flags() {
    return experimental_flags_;
  }
  void set_experimental_flags(const nacl::string& f) {
    experimental_flags_ = f;
  }

  void set_bitcode_hash(const nacl::string& c) {
    bitcode_hash_ = c;
  }

 private:
  // NOTE: There are users of this class that use the copy constructor.
  // Currently the default copy constructor is good enough, but
  // double-check that it is the case when more fields are added.
  bool translate_;
  int8_t opt_level_;
  nacl::string experimental_flags_;
  nacl::string bitcode_hash_;
};

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_OPTIONS_H_
