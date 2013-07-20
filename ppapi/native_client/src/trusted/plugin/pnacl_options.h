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

  // Return |true| if PNaCl is allowed to cache.
  // PNaCl is allowed to cache if the server sends cache validators
  // like Last-Modified time or ETags in the HTTP response, and
  // it does not send "Cache-Control: no-store".
  bool HasCacheKey() const { return (!cache_validators_.empty()); }

  // Return the cache key (which takes into account the bitcode hash,
  // as well as the commandline options).
  nacl::string GetCacheKey() const;

  // Return a character array of \x00 delimited commandline options.
  std::vector<char> GetOptCommandline() const;

  bool translate() const { return translate_; }
  void set_translate(bool t) { translate_ = t; }

  int32_t opt_level() const { return opt_level_; }
  void set_opt_level(int32_t l);

  void set_cache_validators(const nacl::string& c) {
    cache_validators_ = c;
  }

 private:
  // NOTE: There are users of this class that use the copy constructor.
  // Currently the default copy constructor is good enough, but
  // double-check that it is the case when more fields are added.
  bool translate_;
  int32_t opt_level_;
  nacl::string cache_validators_;
};

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_OPTIONS_H_
