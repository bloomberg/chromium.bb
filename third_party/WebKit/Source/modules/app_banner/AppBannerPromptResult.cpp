// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/app_banner/AppBannerPromptResult.h"

namespace blink {

AppBannerPromptResult::~AppBannerPromptResult() {}

String AppBannerPromptResult::outcome() const {
  switch (outcome_) {
    case Outcome::kAccepted:
      return "accepted";

    case Outcome::kDismissed:
      return "dismissed";
  }

  ASSERT_NOT_REACHED();
  return "";
}

AppBannerPromptResult::AppBannerPromptResult(const String& platform,
                                             Outcome outcome)
    : platform_(platform), outcome_(outcome) {}

}  // namespace blink
