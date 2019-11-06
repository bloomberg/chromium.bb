// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hats/hats_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"

HatsHandler::HatsHandler() {}

HatsHandler::~HatsHandler() {}

void HatsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "afterShow", base::BindRepeating(&HatsHandler::HandleAfterShow,
                                       base::Unretained(this)));
}

// Callback for the "afterShow" message.
void HatsHandler::HandleAfterShow(const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION("Feedback.HappinessTrackingSurvey.DesktopResponse",
                            HappinessTrackingSurveyDesktopResponse::kShown);
}
