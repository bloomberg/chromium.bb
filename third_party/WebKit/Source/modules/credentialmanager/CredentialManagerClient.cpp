// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/CredentialManagerClient.h"

#include "core/page/Page.h"

namespace blink {

CredentialManagerClient::CredentialManagerClient(WebCredentialManagerClient* client)
    : m_client(client)
{
}

CredentialManagerClient::~CredentialManagerClient()
{
}

// static
const char* CredentialManagerClient::supplementName()
{
    return "CredentialManagerClient";
}

// static
CredentialManagerClient* CredentialManagerClient::from(Page* page)
{
    return static_cast<CredentialManagerClient*>(WillBeHeapSupplement<Page>::from(page, supplementName()));
}

void provideCredentialManagerClientTo(Page& page, CredentialManagerClient* client)
{
    CredentialManagerClient::provideTo(page, CredentialManagerClient::supplementName(), adoptPtrWillBeNoop(client));
}

void CredentialManagerClient::dispatchFailedSignIn(const WebCredential& credential, WebCredentialManagerClient::NotificationCallbacks* callbacks)
{
    if (!m_client)
        return;
    m_client->dispatchFailedSignIn(credential, callbacks);
}

void CredentialManagerClient::dispatchSignedIn(const WebCredential& credential, WebCredentialManagerClient::NotificationCallbacks* callbacks)
{
    if (!m_client)
        return;
    m_client->dispatchSignedIn(credential, callbacks);
}

void CredentialManagerClient::dispatchSignedOut(WebCredentialManagerClient::NotificationCallbacks* callbacks)
{
    if (!m_client)
        return;
    m_client->dispatchSignedOut(callbacks);
}

void CredentialManagerClient::dispatchRequest(bool zeroClickOnly, const WebVector<WebURL>& federations, WebCredentialManagerClient::RequestCallbacks* callbacks)
{
    if (!m_client)
        return;
    m_client->dispatchRequest(zeroClickOnly, federations, callbacks);
}

} // namespace blink
