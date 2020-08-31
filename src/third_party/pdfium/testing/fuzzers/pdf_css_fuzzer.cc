// Copyright 2016 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "core/fxcrt/css/cfx_css.h"
#include "core/fxcrt/css/cfx_csssyntaxparser.h"
#include "core/fxcrt/fx_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  WideString input =
      WideString::FromUTF8(ByteStringView(data, static_cast<size_t>(size)));

  // If we convert the input into an empty string bail out.
  if (input.IsEmpty())
    return 0;

  CFX_CSSSyntaxParser parser(input.AsStringView());
  CFX_CSSSyntaxStatus status;
  do {
    status = parser.DoSyntaxParse();
  } while (status != CFX_CSSSyntaxStatus::kError &&
           status != CFX_CSSSyntaxStatus::kEOS);
  return 0;
}
