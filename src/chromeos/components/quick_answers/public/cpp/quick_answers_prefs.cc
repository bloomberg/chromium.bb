// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace quick_answers {
namespace prefs {

// A preference that indicates the user has seen the Quick Answers notice.
const char kQuickAnswersNoticed[] = "settings.quick_answers.consented";

// A preference that indicates the user has enabled the Quick Answers services.
const char kQuickAnswersEnabled[] = "settings.quick_answers.enabled";

// A preference to keep track of the number of Quick Answers notice impression.
const char kQuickAnswersNoticeImpressionCount[] =
    "settings.quick_answers.consent.count";

// A preference to keep track of how long (in seconds) the Quick Answers notice
// has shown to the user.
const char kQuickAnswersNoticeImpressionDuration[] =
    "settings.quick_answers.consent.duration";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kQuickAnswersNoticed, false);
  registry->RegisterBooleanPref(kQuickAnswersEnabled, false);
  registry->RegisterIntegerPref(kQuickAnswersNoticeImpressionCount, 0);
  registry->RegisterIntegerPref(kQuickAnswersNoticeImpressionDuration, 0);
}

}  // namespace prefs
}  // namespace quick_answers
}  // namespace chromeos
