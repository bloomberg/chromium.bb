// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_internals_logging_impl.h"

#include "content/public/browser/web_ui.h"

namespace autofill {

content::WebUI* AutofillInternalsLoggingImpl::web_ui_;

AutofillInternalsLoggingImpl::AutofillInternalsLoggingImpl() = default;

AutofillInternalsLoggingImpl::~AutofillInternalsLoggingImpl() = default;

void AutofillInternalsLoggingImpl::LogHelper(const base::Value& message) {
  web_ui_->CallJavascriptFunctionUnsafe("addLog", message);
}

void AutofillInternalsLoggingImpl::set_web_ui(content::WebUI* web_ui) {
  web_ui_ = web_ui;
}

}  // namespace autofill
