// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some code to parse HTTP response headers in the format given by
// PPAPI's ppb_url_response.
// Once we move the trusted NaCl plugin code into chrome,
// we should use the standard net/http/http_response_headers.h code.

// Keep this file very light on dependencies so that it is easy
// build a unittest for this (see the gyp file). Do not depend on anything
// other than the standard libraries.

// NOTE when switching over to net/http/http_response_headers.h:
// There are differences between the "raw" headers that can be parsed by
// net/http/http_response_headers and the headers returned by ppb_url_response.
// The ppb_url_response headers are \n delimited, while the
// http_response_headers are \0 delimited and end in \0\0.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_HTTP_RESPONSE_HEADERS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_HTTP_RESPONSE_HEADERS_H_

#include <string>
#include <utility>
#include <vector>

#include "native_client/src/include/nacl_macros.h"

namespace plugin {

class NaClHttpResponseHeaders {
 public:
  NaClHttpResponseHeaders();
  ~NaClHttpResponseHeaders();

  typedef std::pair<std::string, std::string> Entry;

  // Parse and prepare the headers for use with other methods.
  // Assumes that the headers are \n delimited, which ppb_url_response gives.
  // Invalid header lines are skipped.
  void Parse(const std::string& headers_str);

  // Get the value of the header named |name|
  std::string GetHeader(const std::string& name);

  // Return a concatenated string of HTTP caching validators.
  // E.g., Last-Modified time and ETags.
  std::string GetCacheValidators();

  // Return true if the headers indicate that the data should not be stored.
  bool CacheControlNoStore();

 private:
  std::vector<Entry> header_entries_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NaClHttpResponseHeaders);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_HTTP_RESPONSE_HEADERS_H_
