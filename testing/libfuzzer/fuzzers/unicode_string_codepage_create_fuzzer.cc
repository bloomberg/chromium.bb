// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <vector>

#include "third_party/icu/source/common/unicode/unistr.h"

// Taken from third_party/icu/source/data/mappings/convrtrs.txt file.
static const std::array<const char*, 45> kConverters = {
  {
    "UTF-8",
    "utf-16be",
    "utf-16le",
    "UTF-32",
    "UTF-32BE",
    "UTF-32LE",
    "ibm866-html",
    "iso-8859-2-html",
    "iso-8859-3-html",
    "iso-8859-4-html",
    "iso-8859-5-html",
    "iso-8859-6-html",
    "iso-8859-7-html",
    "iso-8859-8-html",
    "ISO-8859-8-I",
    "iso-8859-10-html",
    "iso-8859-13-html",
    "iso-8859-14-html",
    "iso-8859-15-html",
    "iso-8859-16-html",
    "koi8-r-html",
    "koi8-u-html",
    "macintosh-html",
    "windows-874-html",
    "windows-1250-html",
    "windows-1251-html",
    "windows-1252-html",
    "windows-1253-html",
    "windows-1254-html",
    "windows-1255-html",
    "windows-1256-html",
    "windows-1257-html",
    "windows-1258-html",
    "x-mac-cyrillic-html",
    "windows-936-2000",
    "gb18030",
    "big5-html",
    "euc-jp-html",
    "ISO_2022,locale=ja,version=0",
    "shift_jis-html",
    "euc-kr-html",
    "ISO-2022-KR",
    "ISO-2022-CN",
    "ISO-2022-CN-EXT",
    "HZ-GB-2312"
  }
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  if (size < 2)
    return 0;

  // Need null-terminated string.
  std::vector<char> buffer(size, 0);
  // First byte will be used for random decisions.
  unsigned char selector = data[0];
  size -= 1;
  std::copy(data + 1, data + size, buffer.data());

  // Pointer to a part of fuzzer's data or to some valid codepage value.
  char* codepage;

  if (selector & 1) {
    // Use random codepage value provided by fuzzer (split data in two parts).
    // Remove least significant bit because we already used it as input value.
    size_t codepage_length = (selector >> 1) & 0xF;
    if (size <= codepage_length)
      size /= 2;
    else
      size -= codepage_length;

    codepage = buffer.data() + size;
  } else {
    // Use one of valid codepage values.
    // Remove least significant bit because we already used it as input value.
    size_t index = (selector >> 1) % kConverters.size();
    codepage = const_cast<char*>(kConverters[index]);
  }

  icu::UnicodeString unicode_string(buffer.data(),
                                    static_cast<int>(size),
                                    codepage);

  return 0;
}
