// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions that are shared between the Identity Service implementation and its
// consumers. Currently in //components/signin because they are used by classes
// in this component, which cannot depend on //services/identity to avoid a
// dependency cycle. When these classes have no direct consumers and are moved
// to //services/identity, these functions should correspondingly be moved to
// //services/identity/public/cpp.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_

#include "base/strings/string_piece.h"

class PrefService;

namespace identity {

// Returns true if the username is allowed based on the pattern string.
//
// NOTE: Can be moved to //services/identity/public/cpp once SigninManager is
// moved to //services/identity.
bool IsUsernameAllowedByPattern(base::StringPiece username,
                                base::StringPiece pattern);

// Returns true if the username is either allowed based on a pattern registered
// as |pattern_pref_name| with the preferences service referenced by |prefs|,
// or if such pattern can't be retrieved from |prefs|. This is a legacy
// method intended to be used to migrate from SigninManager::IsAllowedUsername()
// only while SigninManager::Initialize() can still accept a null PrefService*,
// and can be removed once that's no longer the case (see crbug.com/908121).
bool LegacyIsUsernameAllowedByPatternFromPrefs(
    PrefService* prefs,
    const std::string& username,
    const std::string& pattern_pref_name);
}  // namespace identity

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_
