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
#include "bindings/core/v8/V8InspectorFrontendHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendHost.h"
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
    ASSERT(!m_frontendHost);
}

void WebDevToolsFrontendImpl::dispose()
{
    if (m_frontendHost) {
        m_frontendHost->disconnectClient();
        m_frontendHost = nullptr;
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

    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    m_frontendHost = InspectorFrontendHost::create(this, page);
    v8::Handle<v8::Object> global = scriptState->context()->Global();
    v8::Handle<v8::Value> frontendHostObj = toV8(m_frontendHost.get(), global, scriptState->isolate());

    global->Set(v8::String::NewFromUtf8(isolate, "InspectorFrontendHost"), frontendHostObj);
    ScriptController* scriptController = page->mainFrame() ? &page->deprecatedLocalMainFrame()->script() : 0;
    if (scriptController) {
        String installAdditionalAPI =
            "" // Wrap messages that go to embedder.
            "(function(host, methodEntries) {"
            "    host._lastCallId = 0;"
            "    host._callbacks = [];"
            "    host.embedderMessageAck = function(id, error)"
            "    {"
            "        var callback = host._callbacks[id];"
            "        delete host._callbacks[id];"
            "        if (callback)"
            "            callback(error);"
            "    };"
            "    function dispatch(methodName, argumentCount)"
            "    {"
            "        var callId = ++host._lastCallId;"
            "        var argsArray = Array.prototype.slice.call(arguments, 2);"
            "        var callback = argsArray[argsArray.length - 1];"
            "        if (typeof callback === \"function\") {"
            "            argsArray.pop();"
            "            host._callbacks[callId] = callback;"
            "        }"
            "        var message = { \"id\": callId, \"method\": methodName };"
            "        argsArray = argsArray.slice(0, argumentCount);"
            "        if (argsArray.length)"
            "            message.params = argsArray;"
            "        host.sendMessageToEmbedder(JSON.stringify(message));"
            "    };"
            "    methodEntries.forEach(function(methodEntry) { host[methodEntry[0]] = dispatch.bind(null, methodEntry[0], methodEntry[1]); });"
            "})(InspectorFrontendHost,"
            "    [['addFileSystem', 0],"
            "     ['append', 2],"
            "     ['bringToFront', 0],"
            "     ['closeWindow', 0],"
            "     ['loadCompleted', 0],"
            "     ['indexPath', 2],"
            "     ['inspectElementCompleted', 0],"
            "     ['inspectedURLChanged', 1],"
            "     ['moveWindowBy', 2],"
            "     ['openInNewTab', 1],"
            "     ['openUrlOnRemoteDeviceAndInspect', 2],"
            "     ['removeFileSystem', 1],"
            "     ['requestFileSystems', 0],"
            "     ['resetZoom', 0],"
            "     ['save', 3],"
            "     ['searchInPath', 3],"
            "     ['setDeviceCountUpdatesEnabled', 1],"
            "     ['setDevicesUpdatesEnabled', 1],"
            "     ['setWhitelistedShortcuts', 1],"
            "     ['setContentsResizingStrategy', 2],"
            "     ['setInspectedPageBounds', 1],"
            "     ['setIsDocked', 1],"
            "     ['stopIndexing', 1],"
            "     ['zoomIn', 0],"
            "     ['zoomOut', 0]]);"
            ""
            "" // Support for legacy front-ends (<M34). Do not add items here.
            "InspectorFrontendHost.requestSetDockSide = function(dockSide)"
            "{"
            "    InspectorFrontendHost.setIsDocked(dockSide !== \"undocked\");"
            "};"
            "InspectorFrontendHost.supportsFileSystems = function() { return true; };"
            ""
            "" // Support for legacy front-ends (<M28). Do not add items here.
            "InspectorFrontendHost.canInspectWorkers = function() { return true; };"
            "InspectorFrontendHost.canSaveAs = function() { return true; };"
            "InspectorFrontendHost.canSave = function() { return true; };"
            "InspectorFrontendHost.loaded = function() {};"
            "InspectorFrontendHost.hiddenPanels = function() { return ""; };"
            "InspectorFrontendHost.localizedStringsURL = function() { return ""; };"
            "InspectorFrontendHost.close = function(url) { };";
        scriptController->executeScriptInMainWorld(installAdditionalAPI, ScriptController::ExecuteScriptWhenScriptsDisabled);
    }
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
