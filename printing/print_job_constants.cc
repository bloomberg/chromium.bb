// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_job_constants.h"

namespace printing {

// True if this is the first preview request.
const char kIsFirstRequest[] = "isFirstRequest";

// Unique ID sent along every preview request.
const char kPreviewRequestID[] = "requestID";

// Unique ID to identify a print preview UI.
const char kPreviewUIID[] = "previewUIID";

// Print using cloud print: true if selected, false if not.
const char kSettingCloudPrintId[] = "cloudPrintID";

// Print using cloud print dialog: true if selected, false if not.
const char kSettingCloudPrintDialog[] = "printWithCloudPrint";

// Print job setting 'collate'.
const char kSettingCollate[] = "collate";

// Print out color: true for color, false for grayscale.
const char kSettingColor[] = "color";

// Default to color on or not.
const char kSettingSetColorAsDefault[] = "setColorAsDefault";

// Key that specifies the height of the content area of the page.
const char kSettingContentHeight[] = "contentHeight";

// Key that specifies the width of the content area of the page.
const char kSettingContentWidth[] = "contentWidth";

// Number of copies.
const char kSettingCopies[] = "copies";

// Device name: Unique printer identifier.
const char kSettingDeviceName[] = "deviceName";

// Print job duplex mode.
const char kSettingDuplexMode[] = "duplex";

// Option to fit source page contents to printer paper size: true if
// selected else false.
const char kSettingFitToPageEnabled[] = "fitToPageEnabled";

// True, when a new set of draft preview data is required.
const char kSettingGenerateDraftData[] = "generateDraftData";

// Option to print headers and Footers: true if selected, false if not.
const char kSettingHeaderFooterEnabled[] = "headerFooterEnabled";

// Interstice or gap between different header footer components. Hardcoded to
// about 0.5cm, match the value in PrintSettings::SetPrinterPrintableArea.
const float kSettingHeaderFooterInterstice = 14.2f;

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

// Key that specifies the bottom margin of the page.
const char kSettingMarginBottom[] = "marginBottom";

// Key that specifies the left margin of the page.
const char kSettingMarginLeft[] = "marginLeft";

// Key that specifies the right margin of the page.
const char kSettingMarginRight[] = "marginRight";

// Key that specifies the top margin of the page.
const char kSettingMarginTop[] = "marginTop";

// Key that specifies the dictionary of custom margins as set by the user.
const char kSettingMarginsCustom[] = "marginsCustom";

// Key that specifies the type of margins to use.  Value is an int from the
// MarginType enum.
const char kSettingMarginsType[] = "marginsType";

// A page range.
const char kSettingPageRange[] = "pageRange";

// The first page of a page range. (1-based)
const char kSettingPageRangeFrom[] = "from";

// The last page of a page range. (1-based)
const char kSettingPageRangeTo[] = "to";

const char kSettingPreviewModifiable[] = "previewModifiable";

// Keys that specifies the printable area details.
const char kSettingPrintableAreaX[] = "printableAreaX";
const char kSettingPrintableAreaY[] = "printableAreaY";
const char kSettingPrintableAreaWidth[] = "printableAreaWidth";
const char kSettingPrintableAreaHeight[] = "printableAreaHeight";

// Printer name.
const char kSettingPrinterName[] = "printerName";

// Print to PDF option: true if selected, false if not.
const char kSettingPrintToPDF[] = "printToPDF";

// Whether to print CSS backgrounds.
const char kSettingShouldPrintBackgrounds[] = "shouldPrintBackgrounds";

// Indices used to represent first preview page and complete preview document.
const int FIRST_PAGE_INDEX = 0;
const int COMPLETE_PREVIEW_DOCUMENT_INDEX = -1;

#if defined(OS_MACOSX)
const char kSettingOpenPDFInPreview[] = "OpenPDFInPreview";
#endif

#if defined (USE_CUPS)
const char kBlack[] = "Black";
const char kCMYK[] = "CMYK";
const char kKCMY[] = "KCMY";
const char kCMY_K[] = "CMY+K";
const char kCMY[] = "CMY";
const char kColor[] = "Color";
const char kGray[] = "Gray";
const char kGrayscale[] = "Grayscale";
const char kGreyscale[] = "Greyscale";
const char kMonochrome[] = "Monochrome";
const char kNormal[] = "Normal";
const char kNormalGray[] = "Normal.Gray";
const char kRGB[] = "RGB";
const char kRGBA[] = "RGBA";
const char kRGB16[] = "RGB16";
#endif

}  // namespace printing
