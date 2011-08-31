// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_H_

namespace printing {

extern const char kIsFirstRequest[];
extern const char kPreviewRequestID[];
extern const char kPreviewUIAddr[];
extern const char kSettingCloudPrintId[];
extern const char kSettingCollate[];
extern const char kSettingColor[];
extern const char kSettingContentHeight[];
extern const char kSettingContentWidth[];
extern const char kSettingCopies[];
extern const char kSettingDefaultMarginsSelected[];
extern const char kSettingDeviceName[];
extern const char kSettingDuplexMode[];
extern const char kSettingGenerateDraftData[];
extern const char kSettingHeaderFooterEnabled[];
extern const int kSettingHeaderFooterCharacterSpacing;
extern const char kSettingHeaderFooterFontFamilyName[];
extern const char kSettingHeaderFooterFontName[];
extern const int kSettingHeaderFooterFontSize;
extern const float kSettingHeaderFooterHorizontalRegions;
extern const float kSettingHeaderFooterInterstice;
extern const char kSettingHeaderFooterDate[];
extern const char kSettingHeaderFooterTitle[];
extern const char kSettingHeaderFooterURL[];
extern const char kSettingLandscape[];
extern const char kSettingMarginBottom[];
extern const char kSettingMarginLeft[];
extern const char kSettingMarginRight[];
extern const char kSettingMarginTop[];
extern const char kSettingMargins[];
extern const char kSettingPageRange[];
extern const char kSettingPageRangeFrom[];
extern const char kSettingPageRangeTo[];
extern const char kSettingPrinterName[];
extern const char kSettingPrintToPDF[];

extern const int FIRST_PAGE_INDEX;
extern const int COMPLETE_PREVIEW_DOCUMENT_INDEX;

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
