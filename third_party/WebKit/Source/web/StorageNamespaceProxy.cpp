/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageNamespaceProxy.h"

#include "public/platform/Platform.h"
#include "public/platform/WebStorageArea.h"
#include "public/platform/WebStorageNamespace.h"
#include "public/platform/WebString.h"
#include "ChromeClientImpl.h"
#include "StorageAreaProxy.h"
#include "WebKit.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/page/Chrome.h"
#include "core/page/Page.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/MainThread.h"

namespace WebCore {

PassOwnPtr<StorageArea> StorageNamespace::localStorageArea(SecurityOrigin* origin)
{
    ASSERT(isMainThread());
    static WebKit::WebStorageNamespace* localStorageNamespace = 0;
    if (!localStorageNamespace)
        localStorageNamespace = WebKit::Platform::current()->createLocalStorageNamespace();
    return adoptPtr(new StorageAreaProxy(adoptPtr(localStorageNamespace->createStorageArea(origin->toString())), LocalStorage));
}

PassOwnPtr<StorageNamespace> StorageNamespace::sessionStorageNamespace(Page* page)
{
    WebKit::WebViewClient* webViewClient = static_cast<WebKit::WebViewImpl*>(page->chrome().client()->webView())->client();
    return adoptPtr(new StorageNamespaceProxy(adoptPtr(webViewClient->createSessionStorageNamespace())));
}

StorageNamespaceProxy::StorageNamespaceProxy(PassOwnPtr<WebKit::WebStorageNamespace> storageNamespace)
    : m_storageNamespace(storageNamespace)
{
}

StorageNamespaceProxy::~StorageNamespaceProxy()
{
}

PassOwnPtr<StorageArea> StorageNamespaceProxy::storageArea(SecurityOrigin* origin)
{
    return adoptPtr(new StorageAreaProxy(adoptPtr(m_storageNamespace->createStorageArea(origin->toString())), SessionStorage));
}

bool StorageNamespaceProxy::isSameNamespace(const WebKit::WebStorageNamespace& sessionNamespace)
{
    return m_storageNamespace && m_storageNamespace->isSameNamespace(sessionNamespace);
}

} // namespace WebCore
