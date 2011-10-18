// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "base/logging.h"
#include "base/values.h"
#include "printing/page_setup.h"
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
    double top_margin_in_points = 0;
    double bottom_margin_in_points = 0;
    double left_margin_in_points = 0;
    double right_margin_in_points = 0;
    DictionaryValue* custom_margins;
    if (!job_settings.GetDictionary(kSettingMarginsCustom, &custom_margins) ||
        !custom_margins->GetDouble(kSettingMarginTop, &top_margin_in_points) ||
        !custom_margins->GetDouble(kSettingMarginBottom,
                                   &bottom_margin_in_points) ||
        !custom_margins->GetDouble(kSettingMarginLeft,
                                   &left_margin_in_points) ||
        !custom_margins->GetDouble(kSettingMarginRight,
                                   &right_margin_in_points)) {
      NOTREACHED();
    }
    PageMargins margins_in_points;
    margins_in_points.Clear();
    margins_in_points.top = top_margin_in_points;
    margins_in_points.bottom = bottom_margin_in_points;
    margins_in_points.left = left_margin_in_points;
    margins_in_points.right = right_margin_in_points;

    settings_.SetCustomMargins(margins_in_points);
  }

  PrintingContext::Result result = UpdatePrinterSettings(job_settings, ranges);
  PrintSettingsInitializer::InitHeaderFooterStrings(job_settings, &settings_);
  return result;
}

}  // namespace printing
