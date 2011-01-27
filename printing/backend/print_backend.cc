// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

namespace printing {

PrinterBasicInfo::PrinterBasicInfo() : printer_status(0) {}

PrinterBasicInfo::~PrinterBasicInfo() {}

PrinterCapsAndDefaults::PrinterCapsAndDefaults() {}

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() {}

PrintBackend::~PrintBackend() {}

}  // namespace printing
