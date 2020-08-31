// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "chrome/chrome_cleaner/strings/string_test_helpers.h"

namespace chrome_cleaner {

std::vector<wchar_t> CreateVectorWithNulls(const base::string16& str) {
  std::vector<wchar_t> str_with_nulls(str.begin(), str.end());
  std::replace(str_with_nulls.begin(), str_with_nulls.end(), L'0', L'\0');
  // Make sure the resulting vector ends with a null. NtOpenKey requires a
  // null-terminated string even if there are other internal nulls.
  if (str_with_nulls.back() != L'\0')
    str_with_nulls.push_back(L'\0');
  return str_with_nulls;
}

base::string16 FormatVectorWithNulls(const std::vector<wchar_t>& v) {
  std::wostringstream ss;
  for (wchar_t c : v) {
    if (c)
      ss << c;
    else
      ss << L"\\0";
  }
  return ss.str();
}

}  // namespace chrome_cleaner
