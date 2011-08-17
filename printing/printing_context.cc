// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "base/values.h"

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

PrintingContext::Result PrintingContext::OnError() {
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

}  // namespace printing
