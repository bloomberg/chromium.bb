// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_options.h"

#include <iterator>
#include <vector>

#include "native_client/src/include/nacl_string.h"

namespace {

nacl::string ReplaceBadFSChars(nacl::string str,
                               const nacl::string& bad_chars,
                               const nacl::string& replacement) {
  size_t replace_pos = str.find_first_of(bad_chars);
  while (replace_pos != nacl::string::npos) {
    str = str.replace(replace_pos, 1, replacement);
    replace_pos = str.find_first_of(bad_chars);
  }
  return str;
}

}  // namespace

namespace plugin {

// Default to -O0 for now.
PnaclOptions::PnaclOptions() : translate_(false), opt_level_(0) { }

PnaclOptions::~PnaclOptions() {
}

nacl::string PnaclOptions::GetCacheKey() const {
  // TODO(jvoung): We need to read the PNaCl translator's manifest
  // to grab the NaCl / PNaCl ABI version too.
  nacl::stringstream ss;
  // Cast opt_level_ as int so that it doesn't think it's a char.
  ss << "-O:" << static_cast<int>(opt_level_)
     << ";flags:" << experimental_flags_
     <<  ";cache_validators:" << cache_validators_;
  // HTML5 FileSystem-based cache does not allow some characters which
  // may appear in URLs, ETags, or Last-Modified times.  Once we move to
  // our own cache-backend, it will be more tolerant of various cache
  // key values.
  // See: http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
  nacl::string key = ss.str();
  key = ReplaceBadFSChars(key, "/", "_FWDSLASH_");
  key = ReplaceBadFSChars(key, "\\", "_BCKSLASH_");
  key = ReplaceBadFSChars(key, "\0", "_NULL_");
  return key;
}

void PnaclOptions::set_opt_level(int8_t l) {
  if (l < 0) {
    opt_level_ = 0;
    return;
  }
  if (l > 3) {
    opt_level_ = 3;
    return;
  }
  opt_level_ = l;
}

std::vector<char> PnaclOptions::GetOptCommandline() const {
  std::vector<char> result;
  std::vector<nacl::string> tokens;

  // Split the experimental_flags_ + the -On along whitespace.
  // Mostly a copy of "base/strings/string_util.h", but avoid importing
  // base into the PPAPI plugin for now.
  nacl::string delim(" ");
  nacl::string str = experimental_flags_;

  if (opt_level_ != -1) {
    nacl::stringstream ss;
    // Cast as int so that it doesn't think it's a char.
    ss << " -O" << static_cast<int>(opt_level_);
    str += ss.str();
  }

  size_t start = str.find_first_not_of(delim);
  while (start != nacl::string::npos) {
    size_t end = str.find_first_of(delim, start + 1);
    if (end == nacl::string::npos) {
      tokens.push_back(str.substr(start));
      break;
    } else {
      tokens.push_back(str.substr(start, end - start));
      start = str.find_first_not_of(delim, end + 1);
    }
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    nacl::string t = tokens[i];
    result.reserve(result.size() + t.size());
    std::copy(t.begin(), t.end(), std::back_inserter(result));
    result.push_back('\x00');
  }

  return result;
}

}  // namespace plugin
