// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace language {

extern const char kFallbackInputMethodLocale[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
