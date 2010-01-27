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

#include <string>

#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "Document.h"
#include "DOMWindow.h"
#include "Event.h"
#include "Frame.h"
#include "InspectorBackend.h"
#include "InspectorController.h"
#include "InspectorFrontendHost.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8DOMWrapper.h"
#include "V8InspectorFrontendHost.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#undef LOG

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontendClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/src/WebFrameImpl.h"
#include "third_party/WebKit/WebKit/chromium/src/WebViewImpl.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/devtools/devtools_message_data.h"
#include "webkit/glue/devtools/devtools_rpc_js.h"
#include "webkit/glue/devtools/profiler_agent.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsfrontend_impl.h"

using namespace WebCore;
using WebKit::WebDevToolsFrontend;
using WebKit::WebDevToolsFrontendClient;
using WebKit::WebFrame;
using WebKit::WebFrameImpl;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebView;
using WebKit::WebViewImpl;

static v8::Local<v8::String> ToV8String(const String& s)
{
    if (s.isNull())
        return v8::Local<v8::String>();

    return v8::String::New(reinterpret_cast<const uint16_t*>(s.characters()), s.length());
}

DEFINE_RPC_JS_BOUND_OBJ(DebuggerAgent, DEBUGGER_AGENT_STRUCT, DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ProfilerAgent, PROFILER_AGENT_STRUCT, ProfilerAgentDelegate, PROFILER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ToolsAgent, TOOLS_AGENT_STRUCT, ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

class ToolsAgentNativeDelegateImpl : public ToolsAgentNativeDelegate {
public:
    struct ResourceContentRequestData {
        String mimeType;
        RefPtr<Node> frame;
    };

    ToolsAgentNativeDelegateImpl(WebFrameImpl* frame) : m_frame(frame) {}
    virtual ~ToolsAgentNativeDelegateImpl() {}

    // ToolsAgentNativeDelegate implementation.
    virtual void didGetResourceContent(int requestId, const String& content)
    {
        if (!m_resourceContentRequests.contains(requestId)) {
            // This can happen when the redirect source content is reported
            // (after a new Delegate has been created due to JsReset, thus losing the
            // |requestId| put into m_resourceContentRequests by requestSent)
            // which we should ignore. We cannot identify the case relying solely on
            // the |content| (which may or may not be null for various 3xx responses).
            return;
        }
        ResourceContentRequestData request = m_resourceContentRequests.take(requestId);

        InspectorController* ic = m_frame->frame()->page()->inspectorController();
        if (request.frame && request.frame->attached())
            ic->inspectorFrontendHost()->addSourceToFrame(request.mimeType, content, request.frame.get());
    }

    bool waitingForResponse(int resourceId, Node* frame)
    {
        if (m_resourceContentRequests.contains(resourceId)) {
            ASSERT(m_resourceContentRequests.get(resourceId).frame.get() == frame);
            return true;
        }
        return false;
    }

    void requestSent(int resourceId, String mimeType, Node* frame)
    {
        ResourceContentRequestData data;
        data.mimeType = mimeType;
        data.frame = frame;
        ASSERT(!m_resourceContentRequests.contains(resourceId));
        m_resourceContentRequests.set(resourceId, data);
    }

private:
    WebFrameImpl* m_frame;
    HashMap<int, ResourceContentRequestData> m_resourceContentRequests;
};

// static
WebDevToolsFrontend* WebDevToolsFrontend::create(
    WebView* view,
    WebDevToolsFrontendClient* client,
    const WebString& applicationLocale)
{
    return new WebDevToolsFrontendImpl(
      static_cast<WebViewImpl*>(view),
      client,
      webkit_glue::WebStringToString(applicationLocale));
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsFrontendClient* client,
    const String& applicationLocale)
    : m_webViewImpl(webViewImpl)
    , m_client(client)
    , m_applicationLocale(applicationLocale)
    , m_loaded(false)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(frame->frame());

    m_debuggerAgentObj.set(new JsDebuggerAgentBoundObj(this, frameContext, "RemoteDebuggerAgent"));
    m_profilerAgentObj.set(new JsProfilerAgentBoundObj(this, frameContext, "RemoteProfilerAgent"));
    m_toolsAgentObj.set(new JsToolsAgentBoundObj(this, frameContext, "RemoteToolsAgent"));

    // Debugger commands should be sent using special method.
    BoundObject debuggerCommandExecutorObj(frameContext, this, "RemoteDebuggerCommandExecutor");
    debuggerCommandExecutorObj.addProtoFunction(
        "DebuggerCommand",
        WebDevToolsFrontendImpl::jsDebuggerCommand);
    debuggerCommandExecutorObj.addProtoFunction(
        "DebuggerPauseScript",
        WebDevToolsFrontendImpl::jsDebuggerPauseScript);
    debuggerCommandExecutorObj.build();

    BoundObject devToolsHost(frameContext, this, "InspectorFrontendHost");
    devToolsHost.addProtoFunction(
        "reset",
        WebDevToolsFrontendImpl::jsReset);
    devToolsHost.addProtoFunction(
        "addSourceToFrame",
        WebDevToolsFrontendImpl::jsAddSourceToFrame);
    devToolsHost.addProtoFunction(
        "addResourceSourceToFrame",
        WebDevToolsFrontendImpl::jsAddResourceSourceToFrame);
    devToolsHost.addProtoFunction(
        "loaded",
        WebDevToolsFrontendImpl::jsLoaded);
    devToolsHost.addProtoFunction(
        "search",
        WebCore::V8InspectorFrontendHost::searchCallback);
    devToolsHost.addProtoFunction(
        "platform",
        WebDevToolsFrontendImpl::jsPlatform);
    devToolsHost.addProtoFunction(
        "port",
        WebDevToolsFrontendImpl::jsPort);
    devToolsHost.addProtoFunction(
        "activateWindow",
        WebDevToolsFrontendImpl::jsActivateWindow);
    devToolsHost.addProtoFunction(
        "closeWindow",
        WebDevToolsFrontendImpl::jsCloseWindow);
    devToolsHost.addProtoFunction(
        "attach",
        WebDevToolsFrontendImpl::jsDockWindow);
    devToolsHost.addProtoFunction(
        "detach",
        WebDevToolsFrontendImpl::jsUndockWindow);
    devToolsHost.addProtoFunction(
        "localizedStringsURL",
        WebDevToolsFrontendImpl::jsLocalizedStringsURL);
    devToolsHost.addProtoFunction(
        "hiddenPanels",
        WebDevToolsFrontendImpl::jsHiddenPanels);
    devToolsHost.addProtoFunction(
        "setting",
        WebDevToolsFrontendImpl::jsSetting);
    devToolsHost.addProtoFunction(
        "setSetting",
        WebDevToolsFrontendImpl::jsSetSetting);
    devToolsHost.addProtoFunction(
        "windowUnloading",
        WebDevToolsFrontendImpl::jsWindowUnloading);
    devToolsHost.addProtoFunction(
        "showContextMenu",
        WebDevToolsFrontendImpl::jsShowContextMenu);
    devToolsHost.build();
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
    if (m_menuProvider)
        m_menuProvider->disconnect();
}

void WebDevToolsFrontendImpl::dispatchMessageFromAgent(const WebKit::WebDevToolsMessageData& data)
{
    if (ToolsAgentNativeDelegateDispatch::dispatch(m_toolsAgentNativeDelegateImpl.get(), data))
        return;

    Vector<String> v;
    v.append(webkit_glue::WebStringToString(data.className));
    v.append(webkit_glue::WebStringToString(data.methodName));
    for (size_t i = 0; i < data.arguments.size(); i++)
        v.append(webkit_glue::WebStringToString(data.arguments[i]));
    if (!m_loaded) {
        m_pendingIncomingMessages.append(v);
        return;
    }
    executeScript(v);
}

void WebDevToolsFrontendImpl::addResourceSourceToFrame(int resourceId, String mimeType, Node* frame)
{
    if (m_toolsAgentNativeDelegateImpl->waitingForResponse(resourceId, frame))
        return;
    m_toolsAgentObj->getResourceContent(resourceId, resourceId);
    m_toolsAgentNativeDelegateImpl->requestSent(resourceId, mimeType, frame);
}

void WebDevToolsFrontendImpl::executeScript(const Vector<String>& v)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(frame->frame());
    v8::Context::Scope contextScope(frameContext);
    v8::Handle<v8::Value> dispatchFunction = frameContext->Global()->Get(v8::String::New("devtools$$dispatch"));
    ASSERT(dispatchFunction->IsFunction());
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatchFunction);
    Vector< v8::Handle<v8::Value> > args;
    for (size_t i = 0; i < v.size(); i++)
        args.append(ToV8String(v.at(i)));
    function->Call(frameContext->Global(), args.size(), args.data());
}

void WebDevToolsFrontendImpl::dispatchOnWebInspector(const String& methodName, const String& param)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(frame->frame());
    v8::Context::Scope contextScope(frameContext);

    v8::Handle<v8::Value> webInspector = frameContext->Global()->Get(v8::String::New("WebInspector"));
    ASSERT(webInspector->IsObject());
    v8::Handle<v8::Object> webInspectorObj = v8::Handle<v8::Object>::Cast(webInspector);

    v8::Handle<v8::Value> method = webInspectorObj->Get(ToV8String(methodName));
    ASSERT(method->IsFunction());
    v8::Handle<v8::Function> methodFunc = v8::Handle<v8::Function>::Cast(method);
    v8::Handle<v8::Value> args[] = {
      ToV8String(param)
    };
    methodFunc->Call(frameContext->Global(), 1, args);
}

void WebDevToolsFrontendImpl::sendRpcMessage(const WebKit::WebDevToolsMessageData& data)
{
    m_client->sendMessageToAgent(data);
}

void WebDevToolsFrontendImpl::contextMenuItemSelected(ContextMenuItem* item)
{
    int itemNumber = item->action() - ContextMenuItemBaseCustomTag;
    dispatchOnWebInspector("contextMenuItemSelected", String::number(itemNumber));
}

void WebDevToolsFrontendImpl::contextMenuCleared()
{
    dispatchOnWebInspector("contextMenuCleared", "");
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsReset(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(
        v8::External::Cast(*args.Data())->Value());
    WebFrameImpl* frame = frontend->m_webViewImpl->mainFrameImpl();
    frontend->m_toolsAgentNativeDelegateImpl.set(new ToolsAgentNativeDelegateImpl(frame));
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsAddSourceToFrame(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return v8::Undefined();

    v8::TryCatch exceptionCatcher;

    String mimeType = WebCore::toWebCoreStringWithNullCheck(args[0]);
    if (mimeType.isEmpty() || exceptionCatcher.HasCaught())
        return v8::Undefined();

    String sourceString = WebCore::toWebCoreStringWithNullCheck(args[1]);
    if (sourceString.isEmpty() || exceptionCatcher.HasCaught())
        return v8::Undefined();

    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(args[2]);
    Node* node = V8Node::toNative(wrapper);
    if (!node || !node->attached())
        return v8::Undefined();

    Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
    InspectorController* inspectorController = page->inspectorController();
    return WebCore::v8Boolean(inspectorController->inspectorFrontendHost()->
        addSourceToFrame(mimeType, sourceString, node));
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsAddResourceSourceToFrame(const v8::Arguments& args)
{
    int resourceId = static_cast<int>(args[0]->NumberValue());
    String mimeType = WebCore::toWebCoreStringWithNullCheck(args[1]);
    if (mimeType.isEmpty())
        return v8::Undefined();

    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(args[2]);
    Node* node = V8Node::toNative(wrapper);
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(
        v8::External::Cast(*args.Data())->Value());
    frontend->addResourceSourceToFrame(resourceId, mimeType, node);
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsLoaded(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_loaded = true;

    // Grant the devtools page the ability to have source view iframes.
    Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
    SecurityOrigin* origin = page->mainFrame()->domWindow()->securityOrigin();
    origin->grantUniversalAccess();

    for (Vector<Vector<String> >::iterator it = frontend->m_pendingIncomingMessages.begin();
         it != frontend->m_pendingIncomingMessages.end();
         ++it) {
        frontend->executeScript(*it);
    }
    frontend->m_pendingIncomingMessages.clear();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsPlatform(const v8::Arguments& args)
{
#if defined(OS_MACOSX)
    return v8String("mac-leopard");
#elif defined(OS_LINUX)
    return v8String("linux");
#elif defined(OS_FREEBSD)
    return v8String("freebsd");
#elif defined(OS_OPENBSD)
    return v8String("openbsd");
#elif defined(OS_WIN)
    return v8String("windows");
#endif
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsPort(const v8::Arguments& args)
{
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsActivateWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->activateWindow();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsCloseWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->closeWindow();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsDockWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->dockWindow();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsUndockWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->undockWindow();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsLocalizedStringsURL(const v8::Arguments& args)
{
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsHiddenPanels(const v8::Arguments& args)
{
    return v8String("");
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsDebuggerCommand(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    String command = WebCore::toWebCoreStringWithNullCheck(args[0]);
    WebString stdCommand = webkit_glue::StringToWebString(command);
    frontend->m_client->sendDebuggerCommandToAgent(stdCommand);
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsSetting(const v8::Arguments& args)
{
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsSetSetting(const v8::Arguments& args)
{
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsDebuggerPauseScript(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->sendDebuggerPauseScript();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsWindowUnloading(const v8::Arguments& args)
{
    //TODO(pfeldman): Implement this.
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsShowContextMenu(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return v8::Undefined();

    v8::Local<v8::Object> eventWrapper = v8::Local<v8::Object>::Cast(args[0]);
    if (V8DOMWrapper::domWrapperType(eventWrapper) != V8ClassIndex::EVENT)
        return v8::Undefined();

    Event* event = V8Event::toNative(eventWrapper);
    if (!args[1]->IsArray())
        return v8::Undefined();

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(args[1]);
    Vector<ContextMenuItem*> items;

    for (size_t i = 0; i < array->Length(); ++i) {
        v8::Local<v8::Object> item = v8::Local<v8::Object>::Cast(array->Get(v8::Integer::New(i)));
        v8::Local<v8::Value> label = item->Get(v8::String::New("label"));
        v8::Local<v8::Value> id = item->Get(v8::String::New("id"));
        if (label->IsUndefined() || id->IsUndefined()) {
          items.append(new ContextMenuItem(SeparatorType,
                                           ContextMenuItemTagNoAction,
                                           String()));
        } else {
          ContextMenuAction typedId = static_cast<ContextMenuAction>(
              ContextMenuItemBaseCustomTag + id->ToInt32()->Value());
          items.append(new ContextMenuItem(ActionType,
                                           typedId,
                                           toWebCoreStringWithNullCheck(label)));
        }
    }

    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());

    frontend->m_menuProvider = MenuProvider::create(frontend, items);

    ContextMenuController* menuController = frontend->m_webViewImpl->page()->contextMenuController();
    menuController->showContextMenu(event, frontend->m_menuProvider);

    return v8::Undefined();
}
