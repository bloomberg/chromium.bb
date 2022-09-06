// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "printing/units.h"

#if defined(USE_CUPS) && (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS))
#include <cups/cups.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "printing/mojom/print.mojom.h"
#endif

namespace printing {

mojom::ColorModel ColorModeToColorModel(int color_mode) {
  if (color_mode < static_cast<int>(mojom::ColorModel::kUnknownColorModel) ||
      color_mode > static_cast<int>(mojom::ColorModel::kColorModelLast))
    return mojom::ColorModel::kUnknownColorModel;
  return static_cast<mojom::ColorModel>(color_mode);
}

#if defined(USE_CUPS)
void GetColorModelForModel(mojom::ColorModel color_model,
                           std::string* color_setting_name,
                           std::string* color_value) {
#if BUILDFLAG(IS_MAC)
  constexpr char kCUPSColorMode[] = "ColorMode";
  constexpr char kCUPSColorModel[] = "ColorModel";
  constexpr char kCUPSPrintoutMode[] = "PrintoutMode";
  constexpr char kCUPSProcessColorModel[] = "ProcessColorModel";
  constexpr char kCUPSBrotherMonoColor[] = "BRMonoColor";
  constexpr char kCUPSBrotherPrintQuality[] = "BRPrintQuality";
  constexpr char kCUPSEpsonInk[] = "Ink";
  constexpr char kCUPSSharpARCMode[] = "ARCMode";
  constexpr char kCUPSXeroxXRXColor[] = "XRXColor";
#else
  constexpr char kCUPSColorMode[] = "cups-ColorMode";
  constexpr char kCUPSColorModel[] = "cups-ColorModel";
  constexpr char kCUPSPrintoutMode[] = "cups-PrintoutMode";
  constexpr char kCUPSProcessColorModel[] = "cups-ProcessColorModel";
  constexpr char kCUPSBrotherMonoColor[] = "cups-BRMonoColor";
  constexpr char kCUPSBrotherPrintQuality[] = "cups-BRPrintQuality";
  constexpr char kCUPSEpsonInk[] = "cups-Ink";
  constexpr char kCUPSSharpARCMode[] = "cups-ARCMode";
  constexpr char kCUPSXeroxXRXColor[] = "cups-XRXColor";
#endif  // BUILDFLAG(IS_MAC)

  *color_setting_name = kCUPSColorModel;

  switch (color_model) {
    case mojom::ColorModel::kUnknownColorModel:
      *color_value = kGrayscale;
      break;
    case mojom::ColorModel::kGray:
      *color_value = kGray;
      break;
    case mojom::ColorModel::kColor:
      *color_value = kColor;
      break;
    case mojom::ColorModel::kCMYK:
      *color_value = kCMYK;
      break;
    case mojom::ColorModel::kCMY:
      *color_value = kCMY;
      break;
    case mojom::ColorModel::kKCMY:
      *color_value = kKCMY;
      break;
    case mojom::ColorModel::kCMYPlusK:
      *color_value = kCMY_K;
      break;
    case mojom::ColorModel::kBlack:
      *color_value = kBlack;
      break;
    case mojom::ColorModel::kGrayscale:
      *color_value = kGrayscale;
      break;
    case mojom::ColorModel::kRGB:
      *color_value = kRGB;
      break;
    case mojom::ColorModel::kRGB16:
      *color_value = kRGB16;
      break;
    case mojom::ColorModel::kRGBA:
      *color_value = kRGBA;
      break;
    case mojom::ColorModel::kColorModeColor:
      *color_setting_name = kCUPSColorMode;
      *color_value = kColor;
      break;
    case mojom::ColorModel::kColorModeMonochrome:
      *color_setting_name = kCUPSColorMode;
      *color_value = kMonochrome;
      break;
    case mojom::ColorModel::kHPColorColor:
      *color_setting_name = kColor;
      *color_value = kColor;
      break;
    case mojom::ColorModel::kHPColorBlack:
      *color_setting_name = kColor;
      *color_value = kBlack;
      break;
    case mojom::ColorModel::kPrintoutModeNormal:
      *color_setting_name = kCUPSPrintoutMode;
      *color_value = kNormal;
      break;
    case mojom::ColorModel::kPrintoutModeNormalGray:
      *color_setting_name = kCUPSPrintoutMode;
      *color_value = kNormalGray;
      break;
    case mojom::ColorModel::kProcessColorModelCMYK:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kCMYK;
      break;
    case mojom::ColorModel::kProcessColorModelGreyscale:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kGreyscale;
      break;
    case mojom::ColorModel::kProcessColorModelRGB:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kRGB;
      break;
    case mojom::ColorModel::kBrotherCUPSColor:
      *color_setting_name = kCUPSBrotherMonoColor;
      *color_value = kFullColor;
      break;
    case mojom::ColorModel::kBrotherCUPSMono:
      *color_setting_name = kCUPSBrotherMonoColor;
      *color_value = kMono;
      break;
    case mojom::ColorModel::kBrotherBRScript3Color:
      *color_setting_name = kCUPSBrotherPrintQuality;
      *color_value = kColor;
      break;
    case mojom::ColorModel::kBrotherBRScript3Black:
      *color_setting_name = kCUPSBrotherPrintQuality;
      *color_value = kBlack;
      break;
    case mojom::ColorModel::kEpsonInkColor:
      *color_setting_name = kCUPSEpsonInk;
      *color_value = kEpsonColor;
      break;
    case mojom::ColorModel::kEpsonInkMono:
      *color_setting_name = kCUPSEpsonInk;
      *color_value = kEpsonMono;
      break;
    case mojom::ColorModel::kSharpARCModeCMColor:
      *color_setting_name = kCUPSSharpARCMode;
      *color_value = kSharpCMColor;
      break;
    case mojom::ColorModel::kSharpARCModeCMBW:
      *color_setting_name = kCUPSSharpARCMode;
      *color_value = kSharpCMBW;
      break;
    case mojom::ColorModel::kXeroxXRXColorAutomatic:
      *color_setting_name = kCUPSXeroxXRXColor;
      *color_value = kXeroxAutomatic;
      break;
    case mojom::ColorModel::kXeroxXRXColorBW:
      *color_setting_name = kCUPSXeroxXRXColor;
      *color_value = kXeroxBW;
      break;
  }
  // The default case is excluded from the above switch statement to ensure that
  // all ColorModel values are determinantly handled.
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
std::string GetIppColorModelForModel(mojom::ColorModel color_model) {
  // Accept `kUnknownColorModel` for consistency with GetColorModelForModel().
  if (color_model == mojom::ColorModel::kUnknownColorModel)
    return CUPS_PRINT_COLOR_MODE_MONOCHROME;

  absl::optional<bool> is_color = IsColorModelSelected(color_model);
  if (!is_color.has_value()) {
    NOTREACHED();
    return std::string();
  }

  return is_color.value() ? CUPS_PRINT_COLOR_MODE_COLOR
                          : CUPS_PRINT_COLOR_MODE_MONOCHROME;
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#endif  // defined(USE_CUPS)

absl::optional<bool> IsColorModelSelected(mojom::ColorModel color_model) {
  switch (color_model) {
    case mojom::ColorModel::kColor:
    case mojom::ColorModel::kCMYK:
    case mojom::ColorModel::kCMY:
    case mojom::ColorModel::kKCMY:
    case mojom::ColorModel::kCMYPlusK:
    case mojom::ColorModel::kRGB:
    case mojom::ColorModel::kRGB16:
    case mojom::ColorModel::kRGBA:
    case mojom::ColorModel::kColorModeColor:
    case mojom::ColorModel::kHPColorColor:
    case mojom::ColorModel::kPrintoutModeNormal:
    case mojom::ColorModel::kProcessColorModelCMYK:
    case mojom::ColorModel::kProcessColorModelRGB:
    case mojom::ColorModel::kBrotherCUPSColor:
    case mojom::ColorModel::kBrotherBRScript3Color:
    case mojom::ColorModel::kEpsonInkColor:
    case mojom::ColorModel::kSharpARCModeCMColor:
    case mojom::ColorModel::kXeroxXRXColorAutomatic:
      return true;
    case mojom::ColorModel::kGray:
    case mojom::ColorModel::kBlack:
    case mojom::ColorModel::kGrayscale:
    case mojom::ColorModel::kColorModeMonochrome:
    case mojom::ColorModel::kHPColorBlack:
    case mojom::ColorModel::kPrintoutModeNormalGray:
    case mojom::ColorModel::kProcessColorModelGreyscale:
    case mojom::ColorModel::kBrotherCUPSMono:
    case mojom::ColorModel::kBrotherBRScript3Black:
    case mojom::ColorModel::kEpsonInkMono:
    case mojom::ColorModel::kSharpARCModeCMBW:
    case mojom::ColorModel::kXeroxXRXColorBW:
      return false;
    case mojom::ColorModel::kUnknownColorModel:
      NOTREACHED();
      return absl::nullopt;
  }
  // The default case is excluded from the above switch statement to ensure that
  // all ColorModel values are determinantly handled.
}

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq;

PrintSettings::PrintSettings() {
  Clear();
}

PrintSettings::PrintSettings(const PrintSettings& settings) {
  *this = settings;
}

PrintSettings& PrintSettings::operator=(const PrintSettings& settings) {
  if (this == &settings)
    return *this;

  ranges_ = settings.ranges_;
  selection_only_ = settings.selection_only_;
  margin_type_ = settings.margin_type_;
  title_ = settings.title_;
  url_ = settings.url_;
  display_header_footer_ = settings.display_header_footer_;
  should_print_backgrounds_ = settings.should_print_backgrounds_;
  collate_ = settings.collate_;
  color_ = settings.color_;
  copies_ = settings.copies_;
  duplex_mode_ = settings.duplex_mode_;
  device_name_ = settings.device_name_;
  requested_media_ = settings.requested_media_;
  page_setup_device_units_ = settings.page_setup_device_units_;
  dpi_ = settings.dpi_;
  scale_factor_ = settings.scale_factor_;
  rasterize_pdf_ = settings.rasterize_pdf_;
  rasterize_pdf_dpi_ = settings.rasterize_pdf_dpi_;
  landscape_ = settings.landscape_;
  supports_alpha_blend_ = settings.supports_alpha_blend_;
#if BUILDFLAG(IS_WIN)
  printer_language_type_ = settings.printer_language_type_;
#endif
  is_modifiable_ = settings.is_modifiable_;
  pages_per_sheet_ = settings.pages_per_sheet_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  for (const auto& item : settings.advanced_settings_)
    advanced_settings_.emplace(item.first, item.second.Clone());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS)
  send_user_info_ = settings.send_user_info_;
  username_ = settings.username_;
  pin_value_ = settings.pin_value_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  return *this;
}

PrintSettings::~PrintSettings() = default;

void PrintSettings::Clear() {
  ranges_.clear();
  selection_only_ = false;
  margin_type_ = mojom::MarginType::kDefaultMargins;
  title_.clear();
  url_.clear();
  display_header_footer_ = false;
  should_print_backgrounds_ = false;
  collate_ = false;
  color_ = mojom::ColorModel::kUnknownColorModel;
  copies_ = 0;
  duplex_mode_ = mojom::DuplexMode::kUnknownDuplexMode;
  device_name_.clear();
  requested_media_ = RequestedMedia();
  page_setup_device_units_.Clear();
  dpi_ = gfx::Size();
  scale_factor_ = 1.0f;
  rasterize_pdf_ = false;
  rasterize_pdf_dpi_ = 0;
  landscape_ = false;
  supports_alpha_blend_ = true;
#if BUILDFLAG(IS_WIN)
  printer_language_type_ = mojom::PrinterLanguageType::kNone;
#endif
  is_modifiable_ = true;
  pages_per_sheet_ = 1;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  advanced_settings_.clear();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS)
  send_user_info_ = false;
  username_.clear();
  pin_value_.clear();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void PrintSettings::SetPrinterPrintableArea(
    const gfx::Size& physical_size_device_units,
    const gfx::Rect& printable_area_device_units,
    bool landscape_needs_flip) {
  int units_per_inch = device_units_per_inch();
  int header_footer_text_height = 0;
  if (display_header_footer_) {
    // Hard-code text_height = 0.5cm = ~1/5 of inch.
    header_footer_text_height = ConvertUnit(kSettingHeaderFooterInterstice,
                                            kPointsPerInch, units_per_inch);
  }

  PageMargins margins;
  bool small_paper_size = false;
  switch (margin_type_) {
    case mojom::MarginType::kDefaultMargins: {
      // Default margins 1.0cm = ~2/5 of an inch, unless a page dimension is
      // less than 2.54 cm = ~1 inch, in which case set the margins in that
      // dimension to 0.
      static constexpr double kCmInMicrons = 10000;
      int margin_printer_units =
          ConvertUnit(kCmInMicrons, kMicronsPerInch, units_per_inch);
      int min_size_printer_units = units_per_inch;
      margins.header = header_footer_text_height;
      margins.footer = header_footer_text_height;
      if (physical_size_device_units.height() > min_size_printer_units) {
        margins.top = margin_printer_units;
        margins.bottom = margin_printer_units;
      } else {
        margins.top = 0;
        margins.bottom = 0;
        small_paper_size = true;
      }
      if (physical_size_device_units.width() > min_size_printer_units) {
        margins.left = margin_printer_units;
        margins.right = margin_printer_units;
      } else {
        margins.left = 0;
        margins.right = 0;
        small_paper_size = true;
      }
      break;
    }
    case mojom::MarginType::kNoMargins:
    case mojom::MarginType::kPrintableAreaMargins: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = 0;
      margins.bottom = 0;
      margins.left = 0;
      margins.right = 0;
      break;
    }
    case mojom::MarginType::kCustomMargins: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = ConvertUnit(requested_custom_margins_in_points_.top,
                                kPointsPerInch, units_per_inch);
      margins.bottom = ConvertUnit(requested_custom_margins_in_points_.bottom,
                                   kPointsPerInch, units_per_inch);
      margins.left = ConvertUnit(requested_custom_margins_in_points_.left,
                                 kPointsPerInch, units_per_inch);
      margins.right = ConvertUnit(requested_custom_margins_in_points_.right,
                                  kPointsPerInch, units_per_inch);
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  if ((margin_type_ == mojom::MarginType::kDefaultMargins ||
       margin_type_ == mojom::MarginType::kPrintableAreaMargins) &&
      !small_paper_size) {
    page_setup_device_units_.SetRequestedMargins(margins);
  } else {
    page_setup_device_units_.ForceRequestedMargins(margins);
  }
  page_setup_device_units_.Init(physical_size_device_units,
                                printable_area_device_units,
                                header_footer_text_height);
  if (landscape_ && landscape_needs_flip)
    page_setup_device_units_.FlipOrientation();
}

void PrintSettings::SetCustomMargins(
    const PageMargins& requested_margins_in_points) {
  requested_custom_margins_in_points_ = requested_margins_in_points;
  margin_type_ = mojom::MarginType::kCustomMargins;
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
