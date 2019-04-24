// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/strings/grit/components_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

const char kFallbackInputMethodLocale[] = "en-US";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(language::prefs::kAcceptLanguages,
                               l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_CHROMEOS)
  registry->RegisterStringPref(language::prefs::kPreferredLanguages,
                               kFallbackInputMethodLocale);

  registry->RegisterStringPref(language::prefs::kPreferredLanguagesSyncable, "",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
}

}  // namespace language
