// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <algorithm>

#include "third_party/icu/public/common/unicode/uchar.h"
#include "ui/base/text/text_elider.h"

namespace {

const wchar_t kDefaultDocumentTitle[] = L"Untitled Document";
const int kMaxDocumentTitleLength = 50;

}  // namespace

namespace printing {

PrinterBasicInfo::PrinterBasicInfo()
    : printer_status(0),
      is_default(false) {}

PrinterBasicInfo::~PrinterBasicInfo() {}

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults()
    : color_changeable(false),
      duplex_capable(false),
      color_default(false),
      duplex_default(UNKNOWN_DUPLEX_MODE) {}

PrinterSemanticCapsAndDefaults::~PrinterSemanticCapsAndDefaults() {}

PrinterCapsAndDefaults::PrinterCapsAndDefaults() {}

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() {}

PrintBackend::~PrintBackend() {}

string16 PrintBackend::SimplifyDocumentTitle(const string16& title) {
  string16 no_controls(title);
  no_controls.erase(
    std::remove_if(no_controls.begin(), no_controls.end(), &u_iscntrl),
    no_controls.end());
  string16 result;
  ui::ElideString(no_controls, kMaxDocumentTitleLength, &result);
  return result;
}

}  // namespace printing
