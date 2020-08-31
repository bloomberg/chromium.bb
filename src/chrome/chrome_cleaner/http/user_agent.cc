// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/user_agent.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace chrome_cleaner {

namespace {

const base::char16* ArchitectureToString(UserAgent::Architecture architecture) {
  switch (architecture) {
    case UserAgent::WOW64:
      return L"; WOW64";
    case UserAgent::X64:
      return L"; Win64; x64";
    case UserAgent::IA64:
      return L"; Win64; IA64";
    case UserAgent::X86:
      return L"";
    default:
      NOTREACHED();
      return L"";
  }
}

}  // namespace

UserAgent::UserAgent(const base::string16& product_name,
                     const base::string16& product_version)
    : product_name_(product_name),
      product_version_(product_version),
      os_major_version_(0),
      os_minor_version_(0),
      architecture_(X86) {}

UserAgent::~UserAgent() {}

base::string16 UserAgent::AsString() {
  return product_name_ + L"/" + product_version_ + L" (Windows NT " +
         base::NumberToString16(os_major_version_) + L"." +
         base::NumberToString16(os_minor_version_) +
         ArchitectureToString(architecture_) + L") WinHTTP/" + winhttp_version_;
}

}  // namespace chrome_cleaner
