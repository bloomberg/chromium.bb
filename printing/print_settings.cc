// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "printing/units.h"

namespace printing {

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq(base::LINKER_INITIALIZED);

PrintSettings::PrintSettings()
    : min_shrink(1.25),
      max_shrink(2.0),
      desired_dpi(72),
      selection_only(false),
      use_overlays(true),
      display_header_footer(false),
      dpi_(0),
      landscape_(false),
      supports_alpha_blend_(true) {
}

PrintSettings::~PrintSettings() {
}

void PrintSettings::Clear() {
  ranges.clear();
  min_shrink = 1.25;
  max_shrink = 2.;
  desired_dpi = 72;
  selection_only = false;
  date = string16();
  title = string16();
  url = string16();
  display_header_footer = false;
  printer_name_.clear();
  device_name_.clear();
  page_setup_device_units_.Clear();
  dpi_ = 0;
  landscape_ = false;
  supports_alpha_blend_ = true;
}

void PrintSettings::SetPrinterPrintableArea(
    gfx::Size const& physical_size_device_units,
    gfx::Rect const& printable_area_device_units,
    int units_per_inch) {

  int header_footer_text_height = 0;
  int margin_printer_units = 0;
  if (use_overlays) {
    // Hard-code text_height = 0.5cm = ~1/5 of inch.
    header_footer_text_height = ConvertUnit(500, kHundrethsMMPerInch,
                                            units_per_inch);
    // Default margins 1.0cm = ~2/5 of an inch.
    margin_printer_units = ConvertUnit(1000, kHundrethsMMPerInch,
                                       units_per_inch);
  }
  // Start by setting the user configuration
  page_setup_device_units_.Init(physical_size_device_units,
                                printable_area_device_units,
                                header_footer_text_height);


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
  page_setup_device_units_.SetRequestedMargins(margins);
}

bool PrintSettings::Equals(const PrintSettings& rhs) const {
  // Do not test the display device name (printer_name_) for equality since it
  // may sometimes be chopped off at 30 chars. As long as device_name is the
  // same, that's fine.
  return ranges == rhs.ranges &&
      min_shrink == rhs.min_shrink &&
      max_shrink == rhs.max_shrink &&
      desired_dpi == rhs.desired_dpi &&
      device_name_ == rhs.device_name_ &&
      page_setup_device_units_.Equals(rhs.page_setup_device_units_) &&
      dpi_ == rhs.dpi_ &&
      landscape_ == rhs.landscape_;
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

void PrintSettings::SetOrientation(bool landscape) {
  if (landscape_ != landscape) {
    landscape_ = landscape;
    page_setup_device_units_.FlipOrientation();
  }
}

}  // namespace printing
