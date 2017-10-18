// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/fuzzers/pdfium_fuzzer_helper.h"

// TODO(mmoroz,rharrison): remove this after landing of
// https://chromium-review.googlesource.com/c/chromium/src/+/724021
#ifndef DOCTYPE_PDF
#define DOCTYPE_PDF 0
#endif

class PDFiumXFAFuzzer : public PDFiumFuzzerHelper {
 public:
  PDFiumXFAFuzzer() : PDFiumFuzzerHelper() {}
  ~PDFiumXFAFuzzer() override {}

  int GetFormCallbackVersion() const override { return 2; }

  // Return false if XFA doesn't load as otherwise we're duplicating the work
  // done by the non-xfa fuzzer.
  bool OnFormFillEnvLoaded(FPDF_DOCUMENT doc) override {
    int doc_type = DOCTYPE_PDF;
    if (!FPDF_HasXFAField(doc, &doc_type) || doc_type == DOCTYPE_PDF)
      return false;
    return FPDF_LoadXFA(doc);
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  PDFiumXFAFuzzer fuzzer;
  fuzzer.RenderPdf(reinterpret_cast<const char*>(data), size);
  return 0;
}
