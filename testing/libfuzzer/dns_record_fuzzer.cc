// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/dns/dns_response.h"

// Entry point for LibFuzzer.
extern "C" void LLVMFuzzerTestOneInput(const unsigned char* data,
                                       unsigned long size) {
  std::string out;
  net::DnsRecordParser parser(data, size, 0);
  if (!parser.IsValid()) {
    return;
  }
  net::DnsResourceRecord record;
  while (parser.ReadRecord(&record)) {
  }
}
