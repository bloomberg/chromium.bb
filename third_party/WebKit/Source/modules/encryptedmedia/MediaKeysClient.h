// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysClient_h
#define MediaKeysClient_h

#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class Page;
class WebContentDecryptionModule;

class MediaKeysClient {
public:
    virtual PassOwnPtr<WebContentDecryptionModule> createContentDecryptionModule(ExecutionContext*, const String& keySystem) = 0;

protected:
    virtual ~MediaKeysClient() { }
};

} // namespace blink

#endif // MediaKeysClient_h

