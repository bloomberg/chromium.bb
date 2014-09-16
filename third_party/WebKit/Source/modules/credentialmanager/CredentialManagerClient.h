// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialManagerClient_h
#define CredentialManagerClient_h

#include "platform/Supplementable.h"
#include "public/platform/WebCredentialManagerClient.h"
#include "public/platform/WebVector.h"

namespace blink {

class ExecutionContext;
class KURL;
class Page;
class WebCredential;
class WebURL;

// CredentialManagerClient lives as a supplement to Page, and wraps the embedder-provided
// WebCredentialManagerClient's methods to make them visible to the bindings code.
class CredentialManagerClient FINAL : public NoBaseWillBeGarbageCollectedFinalized<CredentialManagerClient>, public WillBeHeapSupplement<Page> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(CredentialManagerClient);
public:
    explicit CredentialManagerClient(WebCredentialManagerClient*);
    virtual ~CredentialManagerClient();

    static const char* supplementName();
    static CredentialManagerClient* from(Page*);
    static CredentialManagerClient* from(ExecutionContext*);

    // Ownership of the callback is transferred to the callee for each of
    // the following methods.
    virtual void dispatchFailedSignIn(const WebCredential&, WebCredentialManagerClient::NotificationCallbacks*);
    virtual void dispatchSignedIn(const WebCredential&, WebCredentialManagerClient::NotificationCallbacks*);
    virtual void dispatchSignedOut(WebCredentialManagerClient::NotificationCallbacks*);
    virtual void dispatchRequest(bool zeroClickOnly, const WebVector<WebURL>& federations, WebCredentialManagerClient::RequestCallbacks*);

private:
    WebCredentialManagerClient* m_client;
};

void provideCredentialManagerClientTo(Page&, CredentialManagerClient*);

} // namespace blink

#endif // CredentialManagerClient_h
