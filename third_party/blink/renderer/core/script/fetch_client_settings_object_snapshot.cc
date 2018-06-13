// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/fetch_client_settings_object_snapshot.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    ExecutionContext& execution_context)
    : FetchClientSettingsObjectSnapshot(execution_context.GetSecurityOrigin(),
                                        execution_context.GetReferrerPolicy()) {
}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const scoped_refptr<const SecurityOrigin> security_origin,
    ReferrerPolicy referrer_policy)
    : security_origin_(std::move(security_origin)),
      referrer_policy_(referrer_policy) {}

}  // namespace blink
