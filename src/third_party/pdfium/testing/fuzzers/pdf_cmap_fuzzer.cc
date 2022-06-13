// Copyright 2016 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "core/fpdfapi/font/cpdf_cmap.h"
#include "core/fxcrt/retain_ptr.h"
#include "third_party/base/span.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 256 * 1024)
    return 0;

  pdfium::MakeRetain<CPDF_CMap>(pdfium::make_span(data, size));
  return 0;
}
