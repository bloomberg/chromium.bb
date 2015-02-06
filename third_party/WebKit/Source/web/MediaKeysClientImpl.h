// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysClientImpl_h
#define MediaKeysClientImpl_h

#include "modules/encryptedmedia/MediaKeysClient.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class WebContentDecryptionModule;
class WebEncryptedMediaClient;

class MediaKeysClientImpl final : public MediaKeysClient {
public:
    MediaKeysClientImpl();

    // MediaKeysClient implementation.
    virtual WebEncryptedMediaClient* encryptedMediaClient(ExecutionContext*) override;
};

} // namespace blink

#endif // MediaKeysClientImpl_h
