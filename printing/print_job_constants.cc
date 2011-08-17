// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_job_constants.h"

namespace printing {

// True if this is the first preview request.
const char kIsFirstRequest[] = "isFirstRequest";

// Unique ID sent along every preview request.
const char kPreviewRequestID[] = "requestID";

// Print using cloud print: true if selected, false if not.
const char kSettingCloudPrintId[] = "cloudPrintID";

// Print job setting 'collate'.
const char kSettingCollate[] = "collate";

// Print out color: true for color, false for grayscale.
const char kSettingColor[] = "color";

// Number of copies.
const char kSettingCopies[] = "copies";

// Device name: Unique printer identifier.
const char kSettingDeviceName[] = "deviceName";

// Print job duplex mode.
const char kSettingDuplexMode[] = "duplex";

// Page orientation: true for landscape, false for portrait.
const char kSettingLandscape[] = "landscape";

// A page range.
const char kSettingPageRange[] = "pageRange";

// The first page of a page range. (1-based)
const char kSettingPageRangeFrom[] = "from";

// The last page of a page range. (1-based)
const char kSettingPageRangeTo[] = "to";

// Printer name.
const char kSettingPrinterName[] = "printerName";

// Print to PDF option: true if selected, false if not.
const char kSettingPrintToPDF[] = "printToPDF";

// Indices used to represent first page, invalid page and complete
// preview document.
const int FIRST_PAGE_INDEX = 0;
const int COMPLETE_PREVIEW_DOCUMENT_INDEX = -1;
const int INVALID_PAGE_INDEX = -2;

}  // namespace printing
