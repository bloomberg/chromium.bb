// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_METRICS_H_

#include <cstddef>

namespace base {
class Value;
}

namespace printing {

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum class PrintDocumentTypeBuckets {
  kHtmlDocument = 0,
  kPdfDocument = 1,
  kMaxValue = kPdfDocument
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum class PrintSettingsBuckets {
  kLandscape = 0,
  kPortrait = 1,
  kColor = 2,
  kBlackAndWhite = 3,
  kCollate = 4,
  kSimplex = 5,
  kDuplex = 6,
  kTotal = 7,
  kHeadersAndFooters = 8,
  kCssBackground = 9,
  kSelectionOnly = 10,
  // kExternalPdfPreview = 11,  // no longer used
  kPageRange = 12,
  kDefaultMedia = 13,
  kNonDefaultMedia = 14,
  kCopies = 15,
  kNonDefaultMargins = 16,
  // kDistillPage = 17,  // no longer used
  kScaling = 18,
  kPrintAsImage = 19,
  kPagesPerSheet = 20,
  kFitToPage = 21,
  kDefaultDpi = 22,
  kNonDefaultDpi = 23,
  kPin = 24,
  kFitToPaper = 25,
  kMaxValue = kFitToPaper
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum class UserActionBuckets {
  kPrintToPrinter = 0,
  kPrintToPdf = 1,
  kCancel = 2,
  kFallbackToAdvancedSettingsDialog = 3,
  kPreviewFailed = 4,
  kPreviewStarted = 5,
  // kInitiatorCrashed = 6,  // no longer used
  kInitiatorClosed = 7,
  kPrintWithCloudPrint = 8,
  kPrintWithPrivet = 9,
  kPrintWithExtension = 10,
  kOpenInMacPreview = 11,
  kPrintToGoogleDrive = 12,
  kMaxValue = kPrintToGoogleDrive
};

// Record the number of local printers.
void ReportNumberOfPrinters(size_t number);

void ReportPrintDocumentTypeAndSizeHistograms(PrintDocumentTypeBuckets doctype,
                                              size_t average_page_size_in_kb);

// Track the popularity of print settings and report the stats.
void ReportPrintSettingsStats(const base::Value& print_settings,
                              const base::Value& preview_settings,
                              bool is_pdf);

// Record the number of times the user requests to regenerate preview data
// before cancelling.
void ReportRegeneratePreviewRequestCountBeforeCancel(size_t count);

// Record the number of times the user requests to regenerate preview data
// before printing.
void ReportRegeneratePreviewRequestCountBeforePrint(size_t count);

void ReportUserActionHistogram(UserActionBuckets event);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_METRICS_H_
