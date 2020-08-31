// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "url/gurl.h"

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

// Empty replacements cause no change when applied.
GURL::Replacements* no_op = new GURL::Replacements();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1)
    return 0;

  base::StringPiece string_piece_input(reinterpret_cast<const char*>(data),
                                       size);
  GURL url_from_string_piece(string_piece_input);
  // Copying by applying empty replacements exercises interesting code paths.
  // This can help discover issues like https://crbug.com/1075515.
  GURL copy = url_from_string_piece.ReplaceComponents(*no_op);
  CHECK_EQ(url_from_string_piece.is_valid(), copy.is_valid());
  if (url_from_string_piece.is_valid()) {
    CHECK_EQ(url_from_string_piece.spec(), copy.spec());
  }

  // Test for StringPiece16 if size is even.
  if (size % 2 == 0) {
    base::StringPiece16 string_piece_input16(
        reinterpret_cast<const base::char16*>(data), size / 2);

    GURL url_from_string_piece16(string_piece_input16);
  }

  // Resolve relative url tests.
  size_t size_t_bytes = sizeof(size_t);
  if (size < size_t_bytes + 1) {
    return 0;
  }
  size_t relative_size =
      *reinterpret_cast<const size_t*>(data) % (size - size_t_bytes);
  std::string relative_string(
      reinterpret_cast<const char*>(data + size_t_bytes), relative_size);
  base::StringPiece string_piece_part_input(
      reinterpret_cast<const char*>(data + size_t_bytes + relative_size),
      size - relative_size - size_t_bytes);
  GURL url_from_string_piece_part(string_piece_part_input);
  url_from_string_piece_part.Resolve(relative_string);

  if (relative_size % 2 == 0) {
    base::string16 relative_string16(
        reinterpret_cast<const base::char16*>(data + size_t_bytes),
        relative_size / 2);
    url_from_string_piece_part.Resolve(relative_string16);
  }
  return 0;
}
