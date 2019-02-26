// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

#include "third_party/blink/renderer/platform/heap/trace_traits.h"

namespace blink {

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData> data)
    : FetchClientSettingsObjectSnapshot(data->base_url,
                                        data->security_origin,
                                        data->referrer_policy,
                                        data->outgoing_referrer,
                                        data->https_state) {}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const KURL& base_url,
    const scoped_refptr<const SecurityOrigin> security_origin,
    network::mojom::ReferrerPolicy referrer_policy,
    const String& outgoing_referrer,
    HttpsState https_state)
    : base_url_(base_url),
      security_origin_(std::move(security_origin)),
      referrer_policy_(referrer_policy),
      outgoing_referrer_(outgoing_referrer),
      https_state_(https_state) {}

}  // namespace blink
