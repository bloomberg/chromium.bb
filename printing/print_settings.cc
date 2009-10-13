// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "printing/units.h"

namespace printing {

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq(base::LINKER_INITIALIZED);

PrintSettings::PrintSettings()
    : min_shrink(1.25),
      max_shrink(2.0),
      desired_dpi(72),
      selection_only(false),
      dpi_(0),
      landscape_(false) {
}

void PrintSettings::Clear() {
  ranges.clear();
  min_shrink = 1.25;
  max_shrink = 2.;
  desired_dpi = 72;
  selection_only = false;
  printer_name_.clear();
  device_name_.clear();
  page_setup_pixels_.Clear();
  dpi_ = 0;
  landscape_ = false;
}

#ifdef WIN32
void PrintSettings::Init(HDC hdc,
                         const DEVMODE& dev_mode,
                         const PageRanges& new_ranges,
                         const std::wstring& new_device_name,
                         bool print_selection_only) {
  DCHECK(hdc);
  printer_name_ = dev_mode.dmDeviceName;
  device_name_ = new_device_name;
  ranges = new_ranges;
  landscape_ = dev_mode.dmOrientation == DMORIENT_LANDSCAPE;
  selection_only = print_selection_only;

  dpi_ = GetDeviceCaps(hdc, LOGPIXELSX);
  // No printer device is known to advertise different dpi in X and Y axis; even
  // the fax device using the 200x100 dpi setting. It's ought to break so many
  // applications that it's not even needed to care about. WebKit doesn't
  // support different dpi settings in X and Y axis.
  DCHECK_EQ(dpi_, GetDeviceCaps(hdc, LOGPIXELSY));

  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORX), 0);
  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORY), 0);

  // Initialize page_setup_pixels_.
  gfx::Size physical_size_pixels(GetDeviceCaps(hdc, PHYSICALWIDTH),
                                 GetDeviceCaps(hdc, PHYSICALHEIGHT));
  gfx::Rect printable_area_pixels(GetDeviceCaps(hdc, PHYSICALOFFSETX),
                                  GetDeviceCaps(hdc, PHYSICALOFFSETY),
                                  GetDeviceCaps(hdc, HORZRES),
                                  GetDeviceCaps(hdc, VERTRES));

  SetPrinterPrintableArea(physical_size_pixels, printable_area_pixels);
}
#elif defined(OS_MACOSX)
void PrintSettings::Init(PMPrinter printer, PMPageFormat page_format,
                         const PageRanges& new_ranges,
                         bool print_selection_only) {
  printer_name_ = base::SysCFStringRefToWide(PMPrinterGetName(printer));
  device_name_ = base::SysCFStringRefToWide(PMPrinterGetID(printer));
  ranges = new_ranges;
  PMOrientation orientation = kPMPortrait;
  PMGetOrientation(page_format, &orientation);
  landscape_ = orientation == kPMLandscape;
  selection_only = print_selection_only;

  UInt32 resolution_count = 0;
  PMResolution best_resolution = { 72.0, 72.0 };
  OSStatus status = PMPrinterGetPrinterResolutionCount(printer,
                                                       &resolution_count);
  if (status == noErr) {
    // Resolution indexes are 1-based.
    for (uint32 i = 1; i <= resolution_count; ++i) {
      PMResolution resolution;
      PMPrinterGetIndexedPrinterResolution(printer, i, &resolution);
      if (best_resolution.hRes > resolution.hRes)
        best_resolution = resolution;
    }
  }
  dpi_ = best_resolution.hRes;
  // See comment in the Windows code above.
  DCHECK_EQ(dpi_, best_resolution.vRes);

  // Get printable area and paper rects (in points)
  PMRect page_rect, paper_rect;
  PMGetAdjustedPageRect(page_format, &page_rect);
  PMGetAdjustedPaperRect(page_format, &paper_rect);
  const double pixels_per_point = dpi_ / 72.0;
  gfx::Size physical_size_pixels(
      (paper_rect.right - paper_rect.left) * pixels_per_point,
      (paper_rect.bottom - paper_rect.top) * pixels_per_point);
  gfx::Rect printable_area_pixels(
      (page_rect.left - paper_rect.left) * pixels_per_point,
      (page_rect.top - paper_rect.top) * pixels_per_point,
      (page_rect.right - page_rect.left) * pixels_per_point,
      (page_rect.bottom - page_rect.top) * pixels_per_point);

  SetPrinterPrintableArea(physical_size_pixels, printable_area_pixels);
}
#endif

void PrintSettings::SetPrinterPrintableArea(
    gfx::Size const& physical_size_pixels,
    gfx::Rect const& printable_area_pixels) {

  // Hard-code text_height = 0.5cm = ~1/5 of inch.
  int header_footer_text_height = ConvertUnit(500, kHundrethsMMPerInch, dpi_);

  // Start by setting the user configuration
  page_setup_pixels_.Init(physical_size_pixels,
                          printable_area_pixels,
                          header_footer_text_height);

  // Default margins 1.0cm = ~2/5 of an inch.
  int margin_printer_units = ConvertUnit(1000, kHundrethsMMPerInch, dpi_);

  // Apply default margins (not user configurable just yet).
  // Since the font height is half the margin we put the header and footers at
  // the font height from the margins.
  PageMargins margins;
  margins.header = header_footer_text_height;
  margins.footer = header_footer_text_height;
  margins.left = margin_printer_units;
  margins.top = margin_printer_units;
  margins.right = margin_printer_units;
  margins.bottom = margin_printer_units;
  page_setup_pixels_.SetRequestedMargins(margins);
}

bool PrintSettings::Equals(const PrintSettings& rhs) const {
  // Do not test the display device name (printer_name_) for equality since it
  // may sometimes be chopped off at 30 chars. As long as device_name is the
  // same, that's fine.
  return ranges == rhs.ranges &&
      min_shrink == rhs.min_shrink &&
      max_shrink == rhs.max_shrink &&
      desired_dpi == rhs.desired_dpi &&
      overlays.Equals(rhs.overlays) &&
      device_name_ == rhs.device_name_ &&
      page_setup_pixels_.Equals(rhs.page_setup_pixels_) &&
      dpi_ == rhs.dpi_ &&
      landscape_ == rhs.landscape_;
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

}  // namespace printing
