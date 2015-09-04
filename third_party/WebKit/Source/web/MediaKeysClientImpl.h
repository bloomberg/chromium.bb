// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysClientImpl_h
#define MediaKeysClientImpl_h

#include "modules/encryptedmedia/MediaKeysClient.h"
#include "wtf/Allocator.h"

namespace blink {

class MediaKeysClientImpl final : public MediaKeysClient {
    DISALLOW_ALLOCATION();
public:
    MediaKeysClientImpl();

    // MediaKeysClient implementation.
    WebEncryptedMediaClient* encryptedMediaClient(ExecutionContext*) override;
};

} // namespace blink

#endif // MediaKeysClientImpl_h
