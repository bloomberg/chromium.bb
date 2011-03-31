// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "base/values.h"
#include "printing/print_job_constants.h"

namespace printing {

PrintingContext::PrintingContext(const std::string& app_locale)
    : dialog_box_dismissed_(false),
      in_print_job_(false),
      abort_printing_(false),
      app_locale_(app_locale) {
}

PrintingContext::~PrintingContext() {
}

void PrintingContext::ResetSettings() {
  ReleaseContext();

  settings_.Clear();

  in_print_job_ = false;
  dialog_box_dismissed_ = false;
  abort_printing_ = false;
}

bool PrintingContext::GetSettingsFromDict(const DictionaryValue& settings,
                                          bool* landscape,
                                          std::string* printerName) {
  bool ret = true;
  if (landscape)
    ret &= settings.GetBoolean(kSettingLandscape, landscape);

  if (printerName)
    ret &= settings.GetString(kSettingPrinterName, printerName);

  return ret;
}

PrintingContext::Result PrintingContext::OnError() {
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

}  // namespace printing
