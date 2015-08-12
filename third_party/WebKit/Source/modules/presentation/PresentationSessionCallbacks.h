// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationSessionCallbacks_h
#define PresentationSessionCallbacks_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PresentationRequest;
class ScriptPromiseResolver;
class WebPresentationSessionClient;
struct WebPresentationError;

// PresentationSessionCallbacks extends WebCallbacks to resolve the underlying
// promise depending on the result passed to the callback. It takes the
// PresentationRequest object that originated the call in its constructor and
// will pass it to the created PresentationSession.
class PresentationSessionCallbacks final
    : public WebCallbacks<WebPassOwnPtr<WebPresentationSessionClient>, const WebPresentationError&> {
public:
    PresentationSessionCallbacks(ScriptPromiseResolver*, PresentationRequest*);
    ~PresentationSessionCallbacks() override = default;

    void onSuccess(WebPassOwnPtr<WebPresentationSessionClient>) override;
    void onError(const WebPresentationError&) override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    Persistent<PresentationRequest> m_request;

    WTF_MAKE_NONCOPYABLE(PresentationSessionCallbacks);
};

} // namespace blink

#endif // PresentationSessionCallbacks_h
