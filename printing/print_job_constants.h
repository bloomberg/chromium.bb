// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_H_

namespace printing {

extern const char kPreviewRequestID[];
extern const char kSettingCloudPrintId[];
extern const char kSettingCollate[];
extern const char kSettingColor[];
extern const char kSettingCopies[];
extern const char kSettingDeviceName[];
extern const char kSettingDuplexMode[];
extern const char kSettingLandscape[];
extern const char kSettingPageRange[];
extern const char kSettingPageRangeFrom[];
extern const char kSettingPageRangeTo[];
extern const char kSettingPrinterName[];
extern const char kSettingPrintToPDF[];

extern const int FIRST_PAGE_INDEX;
extern const int COMPLETE_PREVIEW_DOCUMENT_INDEX;
extern const int INVALID_PAGE_INDEX;

// Print job duplex mode values.
enum DuplexMode {
  SIMPLEX,
  LONG_EDGE,
  SHORT_EDGE,
};

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_H_
