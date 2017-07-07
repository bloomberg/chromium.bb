// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/MediaKeysGetStatusForPolicy.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/MediaKeysPolicy.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

ScriptPromise MediaKeysGetStatusForPolicy::getStatusForPolicy(
    ScriptState* script_state,
    MediaKeys& media_keys,
    MediaKeysPolicy* media_keys_policy) {
  DVLOG(1) << __func__;
  DCHECK(media_keys_policy);

  return media_keys.getStatusForPolicy(script_state, *media_keys_policy);
}

}  // namespace blink
