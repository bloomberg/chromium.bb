// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xfa/fgas/crt/cfgas_stringformatter.h"

#include <stdint.h>

#include <vector>

#include "core/fxcrt/fx_string.h"
#include "public/fpdfview.h"
#include "testing/fuzzers/pdfium_fuzzer_util.h"
#include "testing/fuzzers/xfa_process_state.h"
#include "third_party/base/stl_util.h"
#include "v8/include/cppgc/heap.h"
#include "v8/include/cppgc/persistent.h"
#include "xfa/fxfa/parser/cxfa_localemgr.h"

namespace {

const wchar_t* const kLocales[] = {L"en", L"fr", L"jp", L"zh"};
const CFGAS_StringFormatter::DateTimeType kTypes[] = {
    CFGAS_StringFormatter::DateTimeType::kDate,
    CFGAS_StringFormatter::DateTimeType::kTime,
    CFGAS_StringFormatter::DateTimeType::kDateTime,
    CFGAS_StringFormatter::DateTimeType::kTimeDate};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 5 || size > 128)  // Big strings are unlikely to help.
    return 0;

  auto* state = static_cast<XFAProcessState*>(FPDF_GetFuzzerPerProcessState());
  cppgc::Heap* heap = state->GetHeap();

  uint8_t test_selector = data[0] % 10;
  uint8_t locale_selector = data[1] % pdfium::size(kLocales);
  uint8_t type_selector = data[2] % pdfium::size(kTypes);
  data += 3;
  size -= 3;

  size_t pattern_len = size / 2;
  size_t value_len = size - pattern_len;
  WideString pattern =
      WideString::FromLatin1(ByteStringView(data, pattern_len));
  WideString value =
      WideString::FromLatin1(ByteStringView(data + pattern_len, value_len));

  auto fmt = std::make_unique<CFGAS_StringFormatter>(pattern);

  WideString result;
  CFX_DateTime dt;
  switch (test_selector) {
    case 0:
      fmt->FormatText(value, &result);
      break;
    case 1: {
      auto* mgr = cppgc::MakeGarbageCollected<CXFA_LocaleMgr>(
          heap->GetAllocationHandle(), heap, nullptr,
          kLocales[locale_selector]);
      fmt->FormatNum(mgr, value, &result);
      break;
    }
    case 2: {
      auto* mgr = cppgc::MakeGarbageCollected<CXFA_LocaleMgr>(
          heap->GetAllocationHandle(), heap, nullptr,
          kLocales[locale_selector]);
      fmt->FormatDateTime(mgr, value, kTypes[type_selector], &result);
      break;
    }
    case 3: {
      fmt->FormatNull(&result);
      break;
    }
    case 4: {
      fmt->FormatZero(&result);
      break;
    }
    case 5: {
      fmt->ParseText(value, &result);
      break;
    }
    case 6: {
      auto* mgr = cppgc::MakeGarbageCollected<CXFA_LocaleMgr>(
          heap->GetAllocationHandle(), heap, nullptr,
          kLocales[locale_selector]);
      fmt->ParseNum(mgr, value, &result);
      break;
    }
    case 7: {
      auto* mgr = cppgc::MakeGarbageCollected<CXFA_LocaleMgr>(
          heap->GetAllocationHandle(), heap, nullptr,
          kLocales[locale_selector]);
      fmt->ParseDateTime(mgr, value, kTypes[type_selector], &dt);
      break;
    }
    case 8: {
      fmt->ParseNull(value);
      break;
    }
    case 9: {
      fmt->ParseZero(value);
      break;
    }
  }
  state->ForceGCAndPump();
  return 0;
}
