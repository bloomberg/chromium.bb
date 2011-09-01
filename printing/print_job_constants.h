// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_H_

#include "printing/printing_export.h"

namespace printing {

PRINTING_EXPORT extern const char kIsFirstRequest[];
PRINTING_EXPORT extern const char kPreviewRequestID[];
PRINTING_EXPORT extern const char kPreviewUIAddr[];
PRINTING_EXPORT extern const char kSettingCloudPrintId[];
PRINTING_EXPORT extern const char kSettingCollate[];
PRINTING_EXPORT extern const char kSettingColor[];
PRINTING_EXPORT extern const char kSettingContentHeight[];
PRINTING_EXPORT extern const char kSettingContentWidth[];
PRINTING_EXPORT extern const char kSettingCopies[];
PRINTING_EXPORT extern const char kSettingDefaultMarginsSelected[];
PRINTING_EXPORT extern const char kSettingDeviceName[];
PRINTING_EXPORT extern const char kSettingDuplexMode[];
PRINTING_EXPORT extern const char kSettingGenerateDraftData[];
PRINTING_EXPORT extern const char kSettingHeaderFooterEnabled[];
PRINTING_EXPORT extern const int kSettingHeaderFooterCharacterSpacing;
PRINTING_EXPORT extern const char kSettingHeaderFooterFontFamilyName[];
PRINTING_EXPORT extern const char kSettingHeaderFooterFontName[];
PRINTING_EXPORT extern const int kSettingHeaderFooterFontSize;
PRINTING_EXPORT extern const float kSettingHeaderFooterHorizontalRegions;
PRINTING_EXPORT extern const float kSettingHeaderFooterInterstice;
PRINTING_EXPORT extern const char kSettingHeaderFooterDate[];
PRINTING_EXPORT extern const char kSettingHeaderFooterTitle[];
PRINTING_EXPORT extern const char kSettingHeaderFooterURL[];
PRINTING_EXPORT extern const char kSettingLandscape[];
PRINTING_EXPORT extern const char kSettingMarginBottom[];
PRINTING_EXPORT extern const char kSettingMarginLeft[];
PRINTING_EXPORT extern const char kSettingMarginRight[];
PRINTING_EXPORT extern const char kSettingMarginTop[];
PRINTING_EXPORT extern const char kSettingMargins[];
PRINTING_EXPORT extern const char kSettingPageRange[];
PRINTING_EXPORT extern const char kSettingPageRangeFrom[];
PRINTING_EXPORT extern const char kSettingPageRangeTo[];
PRINTING_EXPORT extern const char kSettingPrinterName[];
PRINTING_EXPORT extern const char kSettingPrintToPDF[];

PRINTING_EXPORT extern const int FIRST_PAGE_INDEX;
PRINTING_EXPORT extern const int COMPLETE_PREVIEW_DOCUMENT_INDEX;

// Print job duplex mode values.
enum DuplexMode {
  SIMPLEX,
  LONG_EDGE,
  SHORT_EDGE,
};

// Specifies the horizontal alignment of the headers and footers.
enum HorizontalHeaderFooterPosition {
  LEFT,
  CENTER,
  RIGHT
};

// Specifies the vertical alignment of the Headers and Footers.
enum VerticalHeaderFooterPosition {
  TOP,
  BOTTOM
};

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_H_
