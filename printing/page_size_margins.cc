// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_size_margins.h"

#include "base/logging.h"
#include "base/values.h"
#include "printing/print_job_constants.h"

namespace printing {

void getCustomMarginsFromJobSettings(const base::DictionaryValue& settings,
                                     PageSizeMargins* page_size_margins) {
  DictionaryValue* custom_margins;
  if (!settings.GetDictionary(kSettingMarginsCustom, &custom_margins) ||
      !custom_margins->GetDouble(kSettingMarginTop,
                                 &page_size_margins->margin_top) ||
      !custom_margins->GetDouble(kSettingMarginBottom,
                                 &page_size_margins->margin_bottom) ||
      !custom_margins->GetDouble(kSettingMarginLeft,
                                 &page_size_margins->margin_left) ||
      !custom_margins->GetDouble(kSettingMarginRight,
                                 &page_size_margins->margin_right)) {
    NOTREACHED();
  }
}

}  // namespace printing
