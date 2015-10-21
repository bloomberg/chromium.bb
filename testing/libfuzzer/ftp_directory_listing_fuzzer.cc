// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"

// Entry point for LibFuzzer.
extern "C" void LLVMFuzzerTestOneInput(const unsigned char* data,
                                       unsigned long size) {
  std::string buffer(reinterpret_cast<const char*>(data), size);
  std::vector<net::FtpDirectoryListingEntry> entries;
  net::ParseFtpDirectoryListing(buffer, base::Time::Now(), &entries);
}
