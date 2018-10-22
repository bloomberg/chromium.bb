// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "net/http/http_security_headers.h"
#include "url/gurl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(data, data + size);
  bool include_subdomains;
  net::HashValueVector spki_hashes;
  GURL report_uri;
  net::ParseHPKPReportOnlyHeader(input, &include_subdomains, &spki_hashes,
                                 &report_uri);
  return 0;
}
