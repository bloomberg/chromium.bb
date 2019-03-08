// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "linux_dev_platform.h"

#include <clocale>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace inspector_protocol {
namespace {
class LinuxDevPlatform : public Platform {
  bool StrToD(const char* str, double* result) const override {
    // This is not thread-safe
    // (see https://en.cppreference.com/w/cpp/locale/setlocale)
    // but good enough for a unittest.
    const char* saved_locale = std::setlocale(LC_NUMERIC, nullptr);
    char* end;
    *result = std::strtod(str, &end);
    std::setlocale(LC_NUMERIC, saved_locale);
    if (errno == ERANGE) {
      // errno must be reset, e.g. see the example here:
      // https://en.cppreference.com/w/cpp/string/byte/strtof
      errno = 0;
      return false;
    }
    return end == str + strlen(str);
  }

  std::unique_ptr<char[]> DToStr(double value) const override {
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << value;
    std::string str = ss.str();
    std::unique_ptr<char[]> result(new char[str.size() + 1]);
    memcpy(result.get(), str.c_str(), str.size() + 1);
    return result;
  }
};
}  // namespace

Platform* GetLinuxDevPlatform() {
  static Platform* deps = new LinuxDevPlatform;
  return deps;
}
}  // namespace inspector_protocol
