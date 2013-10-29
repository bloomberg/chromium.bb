// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "base/logging.h"
#include "base/values.h"
#include "printing/page_setup.h"
#include "printing/page_size_margins.h"
#include "printing/print_settings_initializer.h"

namespace printing {

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

  if (settings_.dpi() == 0)
    UseDefaultSettings();

  if (!PrintSettingsInitializer::InitSettings(job_settings, ranges,
                                              &settings_)) {
    NOTREACHED();
    return OnError();
  }

  bool print_to_pdf = false;
  bool is_cloud_dialog = false;

  if (!job_settings.GetBoolean(kSettingPrintToPDF, &print_to_pdf) ||
      !job_settings.GetBoolean(kSettingCloudPrintDialog, &is_cloud_dialog)) {
    NOTREACHED();
    return OnError();
  }

  bool print_to_cloud = job_settings.HasKey(kSettingCloudPrintId);
  bool open_in_external_preview =
      job_settings.HasKey(kSettingOpenPDFInPreview);

  return UpdatePrinterSettings(
      print_to_pdf || is_cloud_dialog || print_to_cloud,
      open_in_external_preview);
}

}  // namespace printing
