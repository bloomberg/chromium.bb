// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "base/logging.h"
#include "base/values.h"
#include "printing/page_setup.h"
#include "printing/page_size_margins.h"
#include "printing/print_settings_initializer.h"
#include "printing/units.h"

namespace printing {

namespace {
const float kCloudPrintMarginInch = 0.25;
}

PrintingContext::PrintingContext(const std::string& app_locale)
    : dialog_box_dismissed_(false),
      in_print_job_(false),
      abort_printing_(false),
      app_locale_(app_locale) {
}

PrintingContext::~PrintingContext() {
}

void PrintingContext::set_margin_type(MarginType type) {
  DCHECK(type != CUSTOM_MARGINS);
  settings_.set_margin_type(type);
}

void PrintingContext::ResetSettings() {
  ReleaseContext();

  settings_.Clear();

  in_print_job_ = false;
  dialog_box_dismissed_ = false;
  abort_printing_ = false;
}

PrintingContext::Result PrintingContext::OnError() {
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

PrintingContext::Result PrintingContext::UpdatePrintSettings(
    const base::DictionaryValue& job_settings,
    const PageRanges& ranges) {
  ResetSettings();

  if (!PrintSettingsInitializer::InitSettings(job_settings, ranges,
                                              &settings_)) {
    NOTREACHED();
    return OnError();
  }

  bool print_to_pdf = false;
  bool is_cloud_dialog = false;
  bool print_with_privet = false;

  if (!job_settings.GetBoolean(kSettingPrintToPDF, &print_to_pdf) ||
      !job_settings.GetBoolean(kSettingCloudPrintDialog, &is_cloud_dialog) ||
      !job_settings.GetBoolean(kSettingPrintWithPrivet, &print_with_privet)) {
    NOTREACHED();
    return OnError();
  }

  bool print_to_cloud = job_settings.HasKey(kSettingCloudPrintId);
  bool open_in_external_preview =
      job_settings.HasKey(kSettingOpenPDFInPreview);

  if (!open_in_external_preview && (print_to_pdf || print_to_cloud ||
                                    is_cloud_dialog || print_with_privet)) {
    settings_.set_dpi(kDefaultPdfDpi);
    // Cloud print should get size and rect from capabilities received from
    // server.
    gfx::Size paper_size(GetPdfPaperSizeDeviceUnits());
    gfx::Rect paper_rect(0, 0, paper_size.width(), paper_size.height());
    if (print_to_cloud || print_with_privet) {
      paper_rect.Inset(
          kCloudPrintMarginInch * settings_.device_units_per_inch(),
          kCloudPrintMarginInch * settings_.device_units_per_inch());
    }
    DCHECK_EQ(settings_.device_units_per_inch(), kDefaultPdfDpi);
    settings_.SetPrinterPrintableArea(paper_size, paper_rect, true);
    return OK;
  }

  return UpdatePrinterSettings(open_in_external_preview);
}

}  // namespace printing
