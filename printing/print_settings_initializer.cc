// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"

namespace printing {

bool PrintSettingsInitializer::InitSettings(
    const base::DictionaryValue& job_settings,
    const PageRanges& ranges,
    PrintSettings* settings) {
  bool display_header_footer = false;
  if (!job_settings.GetBoolean(kSettingHeaderFooterEnabled,
                               &display_header_footer)) {
    return false;
  }
  settings->set_display_header_footer(display_header_footer);

  if (settings->display_header_footer()) {
    base::string16 title;
    base::string16 url;
    if (!job_settings.GetString(kSettingHeaderFooterTitle, &title) ||
        !job_settings.GetString(kSettingHeaderFooterURL, &url)) {
      return false;
    }
    settings->set_title(title);
    settings->set_url(url);
  }

  bool backgrounds = false;
  bool selection_only = false;
  if (!job_settings.GetBoolean(kSettingShouldPrintBackgrounds, &backgrounds) ||
      !job_settings.GetBoolean(kSettingShouldPrintSelectionOnly,
                               &selection_only)) {
    return false;
  }
  settings->set_should_print_backgrounds(backgrounds);
  settings->set_selection_only(selection_only);

  int margin_type = DEFAULT_MARGINS;
  if (!job_settings.GetInteger(kSettingMarginsType, &margin_type) ||
      (margin_type != DEFAULT_MARGINS &&
       margin_type != NO_MARGINS &&
       margin_type != CUSTOM_MARGINS &&
       margin_type != PRINTABLE_AREA_MARGINS)) {
    margin_type = DEFAULT_MARGINS;
  }
  settings->set_margin_type(static_cast<MarginType>(margin_type));

  if (margin_type == CUSTOM_MARGINS) {
    PageSizeMargins page_size_margins;
    GetCustomMarginsFromJobSettings(job_settings, &page_size_margins);

    PageMargins margins_in_points;
    margins_in_points.Clear();
    margins_in_points.top = page_size_margins.margin_top;
    margins_in_points.bottom = page_size_margins.margin_bottom;
    margins_in_points.left = page_size_margins.margin_left;
    margins_in_points.right = page_size_margins.margin_right;

    settings->SetCustomMargins(margins_in_points);
  }

  settings->set_ranges(ranges);

  int color = 0;
  bool landscape = false;
  int duplex_mode = 0;
  base::string16 device_name;
  bool collate = false;
  int copies = 1;

  if (!job_settings.GetBoolean(kSettingCollate, &collate) ||
      !job_settings.GetInteger(kSettingCopies, &copies) ||
      !job_settings.GetInteger(kSettingColor, &color) ||
      !job_settings.GetInteger(kSettingDuplexMode, &duplex_mode) ||
      !job_settings.GetBoolean(kSettingLandscape, &landscape) ||
      !job_settings.GetString(kSettingDeviceName, &device_name)) {
    return false;
  }

  settings->set_collate(collate);
  settings->set_copies(copies);
  settings->SetOrientation(landscape);
  settings->set_device_name(device_name);
  settings->set_duplex_mode(static_cast<DuplexMode>(duplex_mode));
  settings->set_color(static_cast<ColorModel>(color));

  return true;
}

}  // namespace printing
