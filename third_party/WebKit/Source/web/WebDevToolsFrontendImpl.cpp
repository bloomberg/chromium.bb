/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "web/WebDevToolsFrontendImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8DevToolsHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/DevToolsHost.h"
#include "core/inspector/InspectorController.h"
#include "core/page/Page.h"
#include "public/platform/WebString.h"
#include "public/web/WebDevToolsFrontendClient.h"
#include "web/WebViewImpl.h"

namespace blink {

WebDevToolsFrontend* WebDevToolsFrontend::create(
    WebView* view,
    WebDevToolsFrontendClient* client,
    const WebString& applicationLocale)
{
    return new WebDevToolsFrontendImpl(toWebViewImpl(view), client);
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsFrontendClient* client)
    : m_webViewImpl(webViewImpl)
    , m_client(client)
{
    m_webViewImpl->page()->inspectorController().setInspectorFrontendClient(this);
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
    ASSERT(!m_devtoolsHost);
}

void WebDevToolsFrontendImpl::dispose()
{
    if (m_devtoolsHost) {
        m_devtoolsHost->disconnectClient();
        m_devtoolsHost = nullptr;
    }
    m_client = 0;
}

void WebDevToolsFrontendImpl::windowObjectCleared()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    Page* page = m_webViewImpl->page();
    ASSERT(page->mainFrame());
    ScriptState* scriptState = ScriptState::forMainWorld(page->deprecatedLocalMainFrame());
    ScriptState::Scope scope(scriptState);

    if (m_devtoolsHost)
        m_devtoolsHost->disconnectClient();
    m_devtoolsHost = DevToolsHost::create(this, page);
    v8::Handle<v8::Object> global = scriptState->context()->Global();
    v8::Handle<v8::Value> devtoolsHostObj = toV8(m_devtoolsHost.get(), global, scriptState->isolate());
    global->Set(v8::String::NewFromUtf8(isolate, "DevToolsHost"), devtoolsHostObj);
}

void WebDevToolsFrontendImpl::sendMessageToBackend(const String& message)
{
    if (m_client)
        m_client->sendMessageToBackend(message);
}

void WebDevToolsFrontendImpl::sendMessageToEmbedder(const String& message)
{
    if (m_client)
        m_client->sendMessageToEmbedder(message);
}

bool WebDevToolsFrontendImpl::isUnderTest()
{
    return m_client ? m_client->isUnderTest() : false;
}

} // namespace blink
