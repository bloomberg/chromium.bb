// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

#if defined(USE_CUPS)
void GetColorModelForMode(
    int color_mode, std::string* color_setting_name, std::string* color_value) {
#if defined(OS_MACOSX)
  const char kCUPSColorMode[] = "ColorMode";
  const char kCUPSColorModel[] = "ColorModel";
  const char kCUPSPrintoutMode[] = "PrintoutMode";
  const char kCUPSProcessColorModel[] = "ProcessColorModel";
#else
  const char kCUPSColorMode[] = "cups-ColorMode";
  const char kCUPSColorModel[] = "cups-ColorModel";
  const char kCUPSPrintoutMode[] = "cups-PrintoutMode";
  const char kCUPSProcessColorModel[] = "cups-ProcessColorModel";
#endif  // defined(OS_MACOSX)

  color_setting_name->assign(kCUPSColorModel);
  switch (color_mode) {
    case COLOR:
      color_value->assign(kColor);
      break;
    case CMYK:
      color_value->assign(kCMYK);
      break;
    case PRINTOUTMODE_NORMAL:
      color_value->assign(kNormal);
      color_setting_name->assign(kCUPSPrintoutMode);
      break;
    case PRINTOUTMODE_NORMAL_GRAY:
      color_value->assign(kNormalGray);
      color_setting_name->assign(kCUPSPrintoutMode);
      break;
    case RGB16:
      color_value->assign(kRGB16);
      break;
    case RGBA:
      color_value->assign(kRGBA);
      break;
    case RGB:
      color_value->assign(kRGB);
      break;
    case CMY:
      color_value->assign(kCMY);
      break;
    case CMY_K:
      color_value->assign(kCMY_K);
      break;
    case BLACK:
      color_value->assign(kBlack);
      break;
    case GRAY:
      color_value->assign(kGray);
      break;
    case COLORMODE_COLOR:
      color_setting_name->assign(kCUPSColorMode);
      color_value->assign(kColor);
      break;
    case COLORMODE_MONOCHROME:
      color_setting_name->assign(kCUPSColorMode);
      color_value->assign(kMonochrome);
      break;
    case HP_COLOR_COLOR:
      color_setting_name->assign(kColor);
      color_value->assign(kColor);
      break;
    case HP_COLOR_BLACK:
      color_setting_name->assign(kColor);
      color_value->assign(kBlack);
      break;
    case PROCESSCOLORMODEL_CMYK:
      color_setting_name->assign(kCUPSProcessColorModel);
      color_value->assign(kCMYK);
      break;
    case PROCESSCOLORMODEL_GREYSCALE:
      color_setting_name->assign(kCUPSProcessColorModel);
      color_value->assign(kGreyscale);
      break;
    case PROCESSCOLORMODEL_RGB:
      color_setting_name->assign(kCUPSProcessColorModel);
      color_value->assign(kRGB);
      break;
    default:
      color_value->assign(kGrayscale);
      break;
  }
}
#endif  // defined(USE_CUPS)

bool isColorModelSelected(int model) {
  return (model != GRAY &&
          model != BLACK &&
          model != PRINTOUTMODE_NORMAL_GRAY &&
          model != COLORMODE_MONOCHROME &&
          model != PROCESSCOLORMODEL_GREYSCALE &&
          model != HP_COLOR_BLACK);
}

// Global SequenceNumber used for generating unique cookie values.
static base::StaticAtomicSequenceNumber cookie_seq;

PrintSettings::PrintSettings()
    : min_shrink(1.25),
      max_shrink(2.0),
      desired_dpi(72),
      selection_only(false),
      margin_type(DEFAULT_MARGINS),
      display_header_footer(false),
      should_print_backgrounds(false),
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
  should_print_backgrounds = false;
}

void PrintSettings::SetPrinterPrintableArea(
    gfx::Size const& physical_size_device_units,
    gfx::Rect const& printable_area_device_units,
    int units_per_inch) {
  int header_footer_text_height = 0;
  if (display_header_footer) {
    // Hard-code text_height = 0.5cm = ~1/5 of inch.
    header_footer_text_height = ConvertUnit(kSettingHeaderFooterInterstice,
                                            kPointsPerInch, units_per_inch);
  }

  PageMargins margins;
  switch (margin_type) {
    case DEFAULT_MARGINS: {
      // Default margins 1.0cm = ~2/5 of an inch.
      int margin_printer_units = ConvertUnit(1000, kHundrethsMMPerInch,
                                             units_per_inch);
      margins.header = header_footer_text_height;
      margins.footer = header_footer_text_height;
      margins.top = margin_printer_units;
      margins.bottom = margin_printer_units;
      margins.left = margin_printer_units;
      margins.right = margin_printer_units;
      break;
    }
    case NO_MARGINS:
    case PRINTABLE_AREA_MARGINS: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = 0;
      margins.bottom = 0;
      margins.left = 0;
      margins.right = 0;
      break;
    }
    case CUSTOM_MARGINS: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = ConvertUnitDouble(
          requested_custom_margins_in_points_.top,
          kPointsPerInch,
          units_per_inch);
      margins.bottom = ConvertUnitDouble(
          requested_custom_margins_in_points_.bottom,
          kPointsPerInch,
          units_per_inch);
      margins.left = ConvertUnitDouble(
          requested_custom_margins_in_points_.left,
          kPointsPerInch,
          units_per_inch);
      margins.right = ConvertUnitDouble(
          requested_custom_margins_in_points_.right,
          kPointsPerInch,
          units_per_inch);
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  if (margin_type == DEFAULT_MARGINS || margin_type == PRINTABLE_AREA_MARGINS)
    page_setup_device_units_.SetRequestedMargins(margins);
  else
    page_setup_device_units_.ForceRequestedMargins(margins);

  page_setup_device_units_.Init(physical_size_device_units,
                                printable_area_device_units,
                                header_footer_text_height);
}

void PrintSettings::SetCustomMargins(
    const PageMargins& requested_margins_in_points) {
  requested_custom_margins_in_points_ = requested_margins_in_points;
  margin_type = CUSTOM_MARGINS;
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
      landscape_ == rhs.landscape_ &&
      should_print_backgrounds == rhs.should_print_backgrounds;
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
