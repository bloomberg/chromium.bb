// Copyright 2016 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "core/fxcrt/cfx_widetextbuf.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/fx_string.h"
#include "fxjs/xfa/cfxjse_formcalc_context.h"
#include "testing/fuzzers/pdfium_fuzzer_util.h"
#include "testing/fuzzers/xfa_process_state.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto* state = static_cast<XFAProcessState*>(FPDF_GetFuzzerPerProcessState());
  WideString input = WideString::FromUTF8(ByteStringView(data, size));
  CFXJSE_FormCalcContext::Translate(state->GetHeap(), input.AsStringView());
  state->ForceGCAndPump();
  return 0;
}
