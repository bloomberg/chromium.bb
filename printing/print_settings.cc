// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

#if defined (USE_CUPS)
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
#endif

  color_setting_name->assign(kCUPSColorModel);
  switch (color_mode) {
    case printing::COLOR:
      color_value->assign(printing::kColor);
      break;
    case printing::CMYK:
      color_value->assign(printing::kCMYK);
      break;
    case printing::PRINTOUTMODE_NORMAL:
      color_value->assign(printing::kNormal);
      color_setting_name->assign(kCUPSPrintoutMode);
      break;
    case printing::PRINTOUTMODE_NORMAL_GRAY:
      color_value->assign(printing::kNormalGray);
      color_setting_name->assign(kCUPSPrintoutMode);
      break;
    case printing::RGB16:
      color_value->assign(printing::kRGB16);
      break;
    case printing::RGBA:
      color_value->assign(printing::kRGBA);
      break;
    case printing::RGB:
      color_value->assign(printing::kRGB);
      break;
    case printing::CMY:
      color_value->assign(printing::kCMY);
      break;
    case printing::CMY_K:
      color_value->assign(printing::kCMY_K);
      break;
    case printing::BLACK:
      color_value->assign(printing::kBlack);
      break;
    case printing::GRAY:
      color_value->assign(printing::kGray);
      break;
    case printing::COLORMODE_COLOR:
      color_setting_name->assign(kCUPSColorMode);
      color_value->assign(printing::kColor);
      break;
    case printing::COLORMODE_MONOCHROME:
      color_setting_name->assign(kCUPSColorMode);
      color_value->assign(printing::kMonochrome);
      break;
    case printing::HP_COLOR_COLOR:
      color_setting_name->assign(kColor);
      color_value->assign(printing::kColor);
      break;
    case printing::HP_COLOR_BLACK:
      color_setting_name->assign(kColor);
      color_value->assign(printing::kBlack);
      break;
    case printing::PROCESSCOLORMODEL_CMYK:
      color_setting_name->assign(kCUPSProcessColorModel);
      color_value->assign(printing::kCMYK);
      break;
    case printing::PROCESSCOLORMODEL_GREYSCALE:
      color_setting_name->assign(kCUPSProcessColorModel);
      color_value->assign(printing::kGreyscale);
      break;
    case printing::PROCESSCOLORMODEL_RGB:
      color_setting_name->assign(kCUPSProcessColorModel);
      color_value->assign(printing::kRGB);
      break;
    default:
      color_value->assign(printing::kGrayscale);
      break;
  }
}
#endif

bool isColorModelSelected(int model) {
  return (model != printing::GRAY &&
          model != printing::BLACK &&
          model != printing::PRINTOUTMODE_NORMAL_GRAY &&
          model != printing::COLORMODE_MONOCHROME &&
          model != printing::PROCESSCOLORMODEL_GREYSCALE &&
          model != printing::HP_COLOR_BLACK);
}

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq(base::LINKER_INITIALIZED);

PrintSettings::PrintSettings()
    : min_shrink(1.25),
      max_shrink(2.0),
      desired_dpi(72),
      selection_only(false),
      margin_type(DEFAULT_MARGINS),
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
  if (display_header_footer) {
    // Hard-code text_height = 0.5cm = ~1/5 of inch.
    header_footer_text_height = ConvertUnit(kSettingHeaderFooterInterstice,
                                            kPointsPerInch, units_per_inch);
  }
  page_setup_device_units_.Init(physical_size_device_units,
                                printable_area_device_units,
                                header_footer_text_height);

  PageMargins margins;
  margins.header = header_footer_text_height;
  margins.footer = header_footer_text_height;
  switch (margin_type) {
    case DEFAULT_MARGINS: {
      // Default margins 1.0cm = ~2/5 of an inch.
      int margin_printer_units = ConvertUnit(1000, kHundrethsMMPerInch,
                                             units_per_inch);
      margins.top = margin_printer_units;
      margins.bottom = margin_printer_units;
      margins.left = margin_printer_units;
      margins.right = margin_printer_units;
      break;
    }
    case NO_MARGINS:
    case PRINTABLE_AREA_MARGINS: {
      margins.top = 0;
      margins.bottom = 0;
      margins.left = 0;
      margins.right = 0;
      break;
    }
    case CUSTOM_MARGINS: {
      margins.top = ConvertUnitDouble(custom_margins_in_points_.top,
                                      printing::kPointsPerInch,
                                      units_per_inch);
      margins.bottom = ConvertUnitDouble(custom_margins_in_points_.bottom,
                                         printing::kPointsPerInch,
                                         units_per_inch);
      margins.left = ConvertUnitDouble(custom_margins_in_points_.left,
                                       printing::kPointsPerInch,
                                       units_per_inch);
      margins.right = ConvertUnitDouble(custom_margins_in_points_.right,
                                        printing::kPointsPerInch,
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
}

void PrintSettings::SetCustomMargins(const PageMargins& margins_in_points) {
  custom_margins_in_points_ = margins_in_points;
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
