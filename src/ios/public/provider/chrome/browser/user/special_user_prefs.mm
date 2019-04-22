// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/user/special_user_prefs.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace prefs {

// The detected user type.
const char kSpecialUserType[] = "ios.specialuser.usertype";
// Value of UNKNOWN user type, which is the default value.
const int kSpecialUserTypeUnknown = -1;

}  // namespace prefs
