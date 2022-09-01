// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_UTILS_HASH_H_
#define TESTING_UTILS_HASH_H_

#include <string>

#include "third_party/base/span.h"

std::string CryptToBase16(const uint8_t* digest);
std::string GenerateMD5Base16(pdfium::span<const uint8_t> data);

#endif  // TESTING_UTILS_HASH_H_
