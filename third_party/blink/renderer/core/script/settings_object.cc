// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/settings_object.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

SettingsObject::SettingsObject(ExecutionContext& execution_context)
    : SettingsObject(execution_context.GetSecurityOrigin(),
                     execution_context.GetReferrerPolicy()) {}

SettingsObject::SettingsObject(
    const scoped_refptr<const SecurityOrigin> security_origin,
    ReferrerPolicy referrer_policy)
    : security_origin_(std::move(security_origin)),
      referrer_policy_(referrer_policy) {}

}  // namespace blink
