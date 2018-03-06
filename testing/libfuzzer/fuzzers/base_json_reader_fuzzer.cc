// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#define POISON(address, size) __asan_poison_memory_region(address, size)
#define UNPOISON(address, size) __asan_unpoison_memory_region(address, size)
#elif defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#define POISON(address, size) __msan_poison(address, size)
#define UNPOISON(address, size) __msan_unpoison(address, size)
#else
#define POISON(address, size)
#define UNPOISON(address, size)
#endif

constexpr size_t kPoisonSize = 1024;

int error_code, error_line, error_column;
std::string error_message;

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1)
    return 0;

  // Create a larger buffer than |size|, tell the sanitizer to poison
  // around the edges, and copy the input into the middle. This will help
  // detect buffer over-reads.
  std::unique_ptr<uint8_t[]> input(new uint8_t[size + 2 * kPoisonSize]);
  POISON(input.get(), kPoisonSize);
  POISON(input.get() + kPoisonSize + size, kPoisonSize);
  memcpy(input.get() + kPoisonSize, data, size);

  base::StringPiece input_string(
      reinterpret_cast<char*>(input.get() + kPoisonSize), size);

  const int options = data[size - 1];
  base::JSONReader::ReadAndReturnError(input_string, options, &error_code,
                                       &error_message, &error_line,
                                       &error_column);

  UNPOISON(input.get(), size + 2 * kPoisonSize);

  return 0;
}
