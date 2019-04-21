// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_pref_names.h"

namespace prefs {

// Boolean that is true when offering translate (i.e. the automatic translate
// bubble) is enabled. Even when this is false, the user can force translate
// from the right-click context menu unless translate is disabled by policy.
const char kOfferTranslateEnabled[] = "translate.enabled";

}  // namespace prefs
