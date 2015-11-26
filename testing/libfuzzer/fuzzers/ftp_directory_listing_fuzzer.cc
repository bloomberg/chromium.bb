// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/time/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"

struct TestCase {
  TestCase() {
    CHECK(base::i18n::InitializeICU());
  }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();


// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  std::string buffer(reinterpret_cast<const char*>(data), size);
  std::vector<net::FtpDirectoryListingEntry> entries;
  net::ParseFtpDirectoryListing(buffer, base::Time::Now(), &entries);
  return 0;
}
