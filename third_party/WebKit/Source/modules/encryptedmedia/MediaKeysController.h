// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysController_h
#define MediaKeysController_h

#include "core/page/Page.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class WebContentDecryptionModule;
}

namespace WebCore {

class ExecutionContext;
class MediaKeysClient;

class MediaKeysController FINAL : public Supplement<Page> {
public:
    virtual ~MediaKeysController();

    PassOwnPtr<blink::WebContentDecryptionModule> createContentDecryptionModule(ExecutionContext*, const String& keySystem);

    static void provideMediaKeysTo(Page&, MediaKeysClient*);
    static MediaKeysController* from(Page* page) { return static_cast<MediaKeysController*>(Supplement<Page>::from(page, supplementName())); }

    virtual void trace(Visitor*) OVERRIDE { }

private:
    explicit MediaKeysController(MediaKeysClient*);
    static const char* supplementName();
    MediaKeysClient* m_client;
};

} // namespace WebCore

#endif // MediaKeysController_h

