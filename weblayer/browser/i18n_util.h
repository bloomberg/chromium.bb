// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_I18N_UTIL_H_
#define WEBLAYER_BROWSER_I18N_UTIL_H_

#include <string>

#include "base/callback_list.h"

namespace weblayer {
namespace i18n {

// Returns the currently-in-use ICU locale. This may be called on any thread.
std::string GetApplicationLocale();

// Returns a list of locales suitable for use in the ACCEPT-LANGUAGE header.
// This may be called on any thread.
std::string GetAcceptLangs();

using LocaleChangeSubscription = base::CallbackList<void()>::Subscription;

std::unique_ptr<LocaleChangeSubscription> RegisterLocaleChangeCallback(
    base::RepeatingClosure locale_changed);

}  // namespace i18n
}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_I18N_UTIL_H_
