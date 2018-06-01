// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/settings_object.h"

namespace blink {

SettingsObject* SettingsObject::Create(const SecurityOrigin* security_origin,
                                       ReferrerPolicy referrer_policy) {
  return new SettingsObject(security_origin, referrer_policy);
}

SettingsObject::SettingsObject(const SecurityOrigin* security_origin,
                               ReferrerPolicy referrer_policy)
    : security_origin_(security_origin), referrer_policy_(referrer_policy) {}

}  // namespace blink
