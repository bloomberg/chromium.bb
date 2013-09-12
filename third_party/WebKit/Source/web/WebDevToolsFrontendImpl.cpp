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
#include "WebDevToolsFrontendImpl.h"

#include "InspectorFrontendClientImpl.h"
#include "V8InspectorFrontendHost.h"
#include "V8MouseEvent.h"
#include "V8Node.h"
#include "WebDevToolsFrontendClient.h"
#include "WebFrameImpl.h"
#include "WebScriptSource.h"
#include "WebViewImpl.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/V8Utilities.h"
#include "core/dom/Document.h"
#include "core/dom/Event.h"
#include "core/dom/Node.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendHost.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/ContextMenuItem.h"
#include "core/platform/Pasteboard.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

using namespace WebCore;

namespace WebKit {

class WebDevToolsFrontendImpl::InspectorFrontendResumeObserver : public ActiveDOMObject {
    WTF_MAKE_NONCOPYABLE(InspectorFrontendResumeObserver);
public:
    InspectorFrontendResumeObserver(WebDevToolsFrontendImpl* webDevToolsFrontendImpl, Document* document)
        : ActiveDOMObject(document)
        , m_webDevToolsFrontendImpl(webDevToolsFrontendImpl)
    {
        suspendIfNeeded();
    }

private:
    virtual bool canSuspend() const OVERRIDE
    {
        return true;
    }

    virtual void resume() OVERRIDE
    {
        m_webDevToolsFrontendImpl->resume();
    }

    WebDevToolsFrontendImpl* m_webDevToolsFrontendImpl;
};

WebDevToolsFrontend* WebDevToolsFrontend::create(
    WebView* view,
    WebDevToolsFrontendClient* client,
    const WebString& applicationLocale)
{
    return new WebDevToolsFrontendImpl(toWebViewImpl(view), client, applicationLocale);
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsFrontendClient* client,
    const String& applicationLocale)
    : m_webViewImpl(webViewImpl)
    , m_client(client)
    , m_applicationLocale(applicationLocale)
    , m_inspectorFrontendDispatchTimer(this, &WebDevToolsFrontendImpl::maybeDispatch)
{
    m_webViewImpl->page()->inspectorController().setInspectorFrontendClient(adoptPtr(new InspectorFrontendClientImpl(m_webViewImpl->page(), m_client, this)));

    // Put each DevTools frontend Page into a private group so that it's not
    // deferred along with the inspected page.
    m_webViewImpl->page()->setGroupType(Page::InspectorPageGroup);
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
}

void WebDevToolsFrontendImpl::dispatchOnInspectorFrontend(const WebString& message)
{
    m_messages.append(message);
    maybeDispatch(0);
}

void WebDevToolsFrontendImpl::resume()
{
    // We should call maybeDispatch asynchronously here because we are not allowed to update activeDOMObjects list in
    // resume (See ScriptExecutionContext::resumeActiveDOMObjects).
    if (!m_inspectorFrontendDispatchTimer.isActive())
        m_inspectorFrontendDispatchTimer.startOneShot(0);
}

void WebDevToolsFrontendImpl::maybeDispatch(WebCore::Timer<WebDevToolsFrontendImpl>*)
{
    while (!m_messages.isEmpty()) {
        Document* document = m_webViewImpl->page()->mainFrame()->document();
        if (document->activeDOMObjectsAreSuspended()) {
            m_inspectorFrontendResumeObserver = adoptPtr(new InspectorFrontendResumeObserver(this, document));
            return;
        }
        m_inspectorFrontendResumeObserver.clear();
        doDispatchOnInspectorFrontend(m_messages.takeFirst());
    }
}

void WebDevToolsFrontendImpl::doDispatchOnInspectorFrontend(const WebString& message)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    if (!frame->frame())
        return;
    v8::Isolate* isolate = isolateForFrame(frame->frame());
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Context> frameContext = frame->frame()->script()->currentWorldContext();
    v8::Context::Scope contextScope(frameContext);
    v8::Handle<v8::Value> inspectorFrontendApiValue = frameContext->Global()->Get(v8::String::New("InspectorFrontendAPI"));
    if (!inspectorFrontendApiValue->IsObject())
        return;
    v8::Handle<v8::Object> dispatcherObject = v8::Handle<v8::Object>::Cast(inspectorFrontendApiValue);
    v8::Handle<v8::Value> dispatchFunction = dispatcherObject->Get(v8::String::New("dispatchMessage"));
    // The frame might have navigated away from the front-end page (which is still weird),
    // OR the older version of frontend might have a dispatch method in a different place.
    // FIXME(kaznacheev): Remove when Chrome for Android M18 is retired.
    if (!dispatchFunction->IsFunction()) {
        v8::Handle<v8::Value> inspectorBackendApiValue = frameContext->Global()->Get(v8::String::New("InspectorBackend"));
        if (!inspectorBackendApiValue->IsObject())
            return;
        dispatcherObject = v8::Handle<v8::Object>::Cast(inspectorBackendApiValue);
        dispatchFunction = dispatcherObject->Get(v8::String::New("dispatch"));
        if (!dispatchFunction->IsFunction())
            return;
    }
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatchFunction);
    Vector< v8::Handle<v8::Value> > args;
    args.append(v8String(message, isolate));
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(frame->frame() ? frame->frame()->document() : 0, function, dispatcherObject, args.size(), args.data(), isolate);
}

} // namespace WebKit
