// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

namespace printing {

PrintingContext::PrintingContext()
    : dialog_box_dismissed_(false),
      in_print_job_(false),
      abort_printing_(false) {
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
