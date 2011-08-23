// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_job_constants.h"

namespace printing {

// True if this is the first preview request.
const char kIsFirstRequest[] = "isFirstRequest";

// Unique ID sent along every preview request.
const char kPreviewRequestID[] = "requestID";

// Unique ID to identify a print preview UI.
const char kPreviewUIAddr[] = "previewUIAddr";

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

// Option to print headers and Footers: true if selected, false if not.
const char kSettingHeaderFooterEnabled[] = "headerFooterEnabled";

// Default character spacing for text while printing headers and footers.
// (For CoreGraphics only).
const int kSettingHeaderFooterCharacterSpacing = 0;

// Default font family name for printing the headers and footers.
const char kSettingHeaderFooterFontFamilyName[] = "sans";

// Default font name for printing the headers and footers.
const char kSettingHeaderFooterFontName[] = "Helvetica";

// Default font size for printing the headers and footers.
const int kSettingHeaderFooterFontSize = 8;

// Number of horizontal regions for headers and footers.
const float kSettingHeaderFooterHorizontalRegions = 3;

// Interstice or gap between different header footer components.
// Hardcoded to 0.25cm = 1/10" = 7.2points.
const float kSettingHeaderFooterInterstice = 7.2f;

// Key that specifies the date of the page that will be printed in the headers
// and footers.
const char kSettingHeaderFooterDate[] = "date";

// Key that specifies the title of the page that will be printed in the headers
// and footers.
const char kSettingHeaderFooterTitle[] = "title";

// Key that specifies the URL of the page that will be printed in the headers
// and footers.
const char kSettingHeaderFooterURL[] = "url";

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

// Indices used to represent first preview page and complete preview document.
const int FIRST_PAGE_INDEX = 0;
const int COMPLETE_PREVIEW_DOCUMENT_INDEX = -1;

}  // namespace printing
