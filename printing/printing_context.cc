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
  settings_.margin_type = type;
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

  if (!job_settings.GetBoolean(kSettingHeaderFooterEnabled,
                               &settings_.display_header_footer)) {
    NOTREACHED();
  }

  int margin_type = DEFAULT_MARGINS;
  if (!job_settings.GetInteger(kSettingMarginsType, &margin_type) ||
      (margin_type != DEFAULT_MARGINS &&
       margin_type != NO_MARGINS &&
       margin_type != CUSTOM_MARGINS &&
       margin_type != PRINTABLE_AREA_MARGINS)) {
    NOTREACHED();
  }
  settings_.margin_type = static_cast<MarginType>(margin_type);

  if (margin_type == CUSTOM_MARGINS) {
    printing::PageSizeMargins page_size_margins;
    GetCustomMarginsFromJobSettings(job_settings, &page_size_margins);

    PageMargins margins_in_points;
    margins_in_points.Clear();
    margins_in_points.top = page_size_margins.margin_top;
    margins_in_points.bottom = page_size_margins.margin_bottom;
    margins_in_points.left = page_size_margins.margin_left;
    margins_in_points.right = page_size_margins.margin_right;

    settings_.SetCustomMargins(margins_in_points);
  }

  PrintingContext::Result result = UpdatePrinterSettings(job_settings, ranges);
  PrintSettingsInitializer::InitHeaderFooterStrings(job_settings, &settings_);

  if (!job_settings.GetBoolean(kSettingShouldPrintBackgrounds,
                               &settings_.should_print_backgrounds)) {
    NOTREACHED();
  }

  return result;
}

}  // namespace printing
