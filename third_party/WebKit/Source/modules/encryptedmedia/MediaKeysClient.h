// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysClient_h
#define MediaKeysClient_h

#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class WebContentDecryptionModule;
class WebEncryptedMediaClient;

class MediaKeysClient {
public:
    // FIXME: remove once encryptedMediaClient() is used.
    virtual PassOwnPtr<WebContentDecryptionModule> createContentDecryptionModule(ExecutionContext*, const String& keySystem) = 0;

    virtual WebEncryptedMediaClient* encryptedMediaClient(ExecutionContext*) = 0;

protected:
    virtual ~MediaKeysClient() { }
};

} // namespace blink

#endif // MediaKeysClient_h

