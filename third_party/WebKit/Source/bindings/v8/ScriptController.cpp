/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "bindings/v8/ScriptController.h"

#include "V8Event.h"
#include "V8HTMLElement.h"
#include "V8Window.h"
#include "bindings/v8/BindingSecurity.h"
#include "bindings/v8/NPV8Object.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8NPObject.h"
#include "bindings/v8/V8PerContextData.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/V8WindowShell.h"
#include "bindings/v8/npruntime_impl.h"
#include "bindings/v8/npruntime_priv.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/plugins/PluginView.h"
#include "platform/NotImplemented.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/Widget.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

bool ScriptController::canAccessFromCurrentOrigin(LocalFrame *frame)
{
    if (!frame)
        return false;
    v8::Isolate* isolate = toIsolate(frame);
    return !isolate->InContext() || BindingSecurity::shouldAllowAccessToFrame(isolate, frame);
}

ScriptController::ScriptController(LocalFrame* frame)
    : m_frame(frame)
    , m_sourceURL(0)
    , m_isolate(v8::Isolate::GetCurrent())
    , m_windowShell(V8WindowShell::create(frame, DOMWrapperWorld::mainWorld(), m_isolate))
    , m_windowScriptNPObject(0)
{
}

ScriptController::~ScriptController()
{
    // V8WindowShell::clearForClose() must be invoked before destruction starts.
    ASSERT(!m_windowShell->isContextInitialized());
}

void ScriptController::clearScriptObjects()
{
    PluginObjectMap::iterator it = m_pluginObjects.begin();
    for (; it != m_pluginObjects.end(); ++it) {
        _NPN_UnregisterObject(it->value);
        _NPN_ReleaseObject(it->value);
    }
    m_pluginObjects.clear();

    if (m_windowScriptNPObject) {
        // Dispose of the underlying V8 object before releasing our reference
        // to it, so that if a plugin fails to release it properly we will
        // only leak the NPObject wrapper, not the object, its document, or
        // anything else they reference.
        disposeUnderlyingV8Object(m_windowScriptNPObject, m_isolate);
        _NPN_ReleaseObject(m_windowScriptNPObject);
        m_windowScriptNPObject = 0;
    }
}

void ScriptController::clearForClose()
{
    double start = currentTime();
    m_windowShell->clearForClose();
    for (IsolatedWorldMap::iterator iter = m_isolatedWorlds.begin(); iter != m_isolatedWorlds.end(); ++iter)
        iter->value->clearForClose();
    blink::Platform::current()->histogramCustomCounts("WebCore.ScriptController.clearForClose", (currentTime() - start) * 1000, 0, 10000, 50);
}

void ScriptController::updateSecurityOrigin(SecurityOrigin* origin)
{
    m_windowShell->updateSecurityOrigin(origin);
}

v8::Local<v8::Value> ScriptController::callFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> info[])
{
    // Keep LocalFrame (and therefore ScriptController) alive.
    RefPtr<LocalFrame> protect(m_frame);
    return ScriptController::callFunction(m_frame->document(), function, receiver, argc, info, m_isolate);
}

static bool resourceInfo(const v8::Handle<v8::Function> function, String& resourceName, int& lineNumber)
{
    v8::ScriptOrigin origin = function->GetScriptOrigin();
    if (origin.ResourceName().IsEmpty()) {
        resourceName = "undefined";
        lineNumber = 1;
    } else {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringResourceName, origin.ResourceName(), false);
        resourceName = stringResourceName;
        lineNumber = function->GetScriptLineNumber() + 1;
    }
    return true;
}

v8::Local<v8::Value> ScriptController::callFunction(ExecutionContext* context, v8::Handle<v8::Function> function, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> info[], v8::Isolate* isolate)
{
    InspectorInstrumentationCookie cookie;
    if (InspectorInstrumentation::timelineAgentEnabled(context)) {
        String resourceName;
        int lineNumber;
        if (!resourceInfo(getBoundFunction(function), resourceName, lineNumber))
            return v8::Local<v8::Value>();
        cookie = InspectorInstrumentation::willCallFunction(context, resourceName, lineNumber);
    }

    v8::Local<v8::Value> result = V8ScriptRunner::callFunction(function, context, receiver, argc, info, isolate);

    InspectorInstrumentation::didCallFunction(cookie);
    return result;
}

v8::Local<v8::Value> ScriptController::executeScriptAndReturnValue(v8::Handle<v8::Context> context, const ScriptSourceCode& source, AccessControlStatus corsStatus)
{
    v8::Context::Scope scope(context);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willEvaluateScript(m_frame, source.url().isNull() ? String() : source.url().string(), source.startLine());

    v8::Local<v8::Value> result;
    {
        // Isolate exceptions that occur when compiling and executing
        // the code. These exceptions should not interfere with
        // javascript code we might evaluate from C++ when returning
        // from here.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        v8::Handle<v8::Script> script = V8ScriptRunner::compileScript(source, m_isolate, corsStatus);

        // Keep LocalFrame (and therefore ScriptController) alive.
        RefPtr<LocalFrame> protect(m_frame);
        result = V8ScriptRunner::runCompiledScript(script, m_frame->document(), m_isolate);
        ASSERT(!tryCatch.HasCaught() || result.IsEmpty());
    }

    InspectorInstrumentation::didEvaluateScript(cookie);

    return result;
}

bool ScriptController::initializeMainWorld()
{
    if (m_windowShell->isContextInitialized())
        return false;
    return windowShell(DOMWrapperWorld::mainWorld())->isContextInitialized();
}

V8WindowShell* ScriptController::existingWindowShell(DOMWrapperWorld* world)
{
    ASSERT(world);

    if (world->isMainWorld())
        return m_windowShell->isContextInitialized() ? m_windowShell.get() : 0;

    IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(world->worldId());
    if (iter == m_isolatedWorlds.end())
        return 0;
    return iter->value->isContextInitialized() ? iter->value.get() : 0;
}

V8WindowShell* ScriptController::windowShell(DOMWrapperWorld* world)
{
    ASSERT(world);

    V8WindowShell* shell = 0;
    if (world->isMainWorld())
        shell = m_windowShell.get();
    else {
        IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(world->worldId());
        if (iter != m_isolatedWorlds.end())
            shell = iter->value.get();
        else {
            OwnPtr<V8WindowShell> isolatedWorldShell = V8WindowShell::create(m_frame, world, m_isolate);
            shell = isolatedWorldShell.get();
            m_isolatedWorlds.set(world->worldId(), isolatedWorldShell.release());
        }
    }
    if (!shell->isContextInitialized() && shell->initializeIfNeeded())
        m_frame->loader().dispatchDidClearWindowObjectInWorld(world);
    return shell;
}

bool ScriptController::shouldBypassMainWorldContentSecurityPolicy()
{
    v8::Handle<v8::Context> context = m_isolate->GetCurrentContext();
    if (context.IsEmpty() || !toDOMWindow(context))
        return false;
    DOMWrapperWorld* world = DOMWrapperWorld::current(m_isolate);
    return world->isIsolatedWorld() ? world->isolatedWorldHasContentSecurityPolicy() : false;
}

TextPosition ScriptController::eventHandlerPosition() const
{
    ScriptableDocumentParser* parser = m_frame->document()->scriptableDocumentParser();
    if (parser)
        return parser->textPosition();
    return TextPosition::minimumPosition();
}

// Create a V8 object with an interceptor of NPObjectPropertyGetter.
void ScriptController::bindToWindowObject(LocalFrame* frame, const String& key, NPObject* object)
{
    v8::HandleScope handleScope(m_isolate);

    v8::Handle<v8::Context> v8Context = toV8Context(m_isolate, frame, DOMWrapperWorld::mainWorld());
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);

    v8::Handle<v8::Object> value = createV8ObjectForNPObject(object, 0, m_isolate);

    // Attach to the global object.
    v8::Handle<v8::Object> global = v8Context->Global();
    global->Set(v8String(m_isolate, key), value);
}

void ScriptController::enableEval()
{
    if (!m_windowShell->isContextInitialized())
        return;
    v8::HandleScope handleScope(m_isolate);
    m_windowShell->context()->AllowCodeGenerationFromStrings(true);
}

void ScriptController::disableEval(const String& errorMessage)
{
    if (!m_windowShell->isContextInitialized())
        return;
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Context> v8Context = m_windowShell->context();
    v8Context->AllowCodeGenerationFromStrings(false);
    v8Context->SetErrorMessageForCodeGenerationFromStrings(v8String(m_isolate, errorMessage));
}

PassRefPtr<SharedPersistent<v8::Object> > ScriptController::createPluginWrapper(Widget* widget)
{
    ASSERT(widget);

    if (!widget->isPluginView())
        return nullptr;

    NPObject* npObject = toPluginView(widget)->scriptableObject();
    if (!npObject)
        return nullptr;

    // LocalFrame Memory Management for NPObjects
    // -------------------------------------
    // NPObjects are treated differently than other objects wrapped by JS.
    // NPObjects can be created either by the browser (e.g. the main
    // window object) or by the plugin (the main plugin object
    // for a HTMLEmbedElement). Further, unlike most DOM Objects, the frame
    // is especially careful to ensure NPObjects terminate at frame teardown because
    // if a plugin leaks a reference, it could leak its objects (or the browser's objects).
    //
    // The LocalFrame maintains a list of plugin objects (m_pluginObjects)
    // which it can use to quickly find the wrapped embed object.
    //
    // Inside the NPRuntime, we've added a few methods for registering
    // wrapped NPObjects. The purpose of the registration is because
    // javascript garbage collection is non-deterministic, yet we need to
    // be able to tear down the plugin objects immediately. When an object
    // is registered, javascript can use it. When the object is destroyed,
    // or when the object's "owning" object is destroyed, the object will
    // be un-registered, and the javascript engine must not use it.
    //
    // Inside the javascript engine, the engine can keep a reference to the
    // NPObject as part of its wrapper. However, before accessing the object
    // it must consult the _NPN_Registry.

    v8::Local<v8::Object> wrapper = createV8ObjectForNPObject(npObject, 0, m_isolate);

    // Track the plugin object. We've been given a reference to the object.
    m_pluginObjects.set(widget, npObject);

    return SharedPersistent<v8::Object>::create(wrapper, m_isolate);
}

void ScriptController::cleanupScriptObjectsForPlugin(Widget* nativeHandle)
{
    PluginObjectMap::iterator it = m_pluginObjects.find(nativeHandle);
    if (it == m_pluginObjects.end())
        return;
    _NPN_UnregisterObject(it->value);
    _NPN_ReleaseObject(it->value);
    m_pluginObjects.remove(it);
}

V8Extensions& ScriptController::registeredExtensions()
{
    DEFINE_STATIC_LOCAL(V8Extensions, extensions, ());
    return extensions;
}

void ScriptController::registerExtensionIfNeeded(v8::Extension* extension)
{
    const V8Extensions& extensions = registeredExtensions();
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (extensions[i] == extension)
            return;
    }
    v8::RegisterExtension(extension);
    registeredExtensions().append(extension);
}

static NPObject* createNoScriptObject()
{
    notImplemented();
    return 0;
}

static NPObject* createScriptObject(LocalFrame* frame, v8::Isolate* isolate)
{
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> v8Context = toV8Context(isolate, frame, DOMWrapperWorld::mainWorld());
    if (v8Context.IsEmpty())
        return createNoScriptObject();

    v8::Context::Scope scope(v8Context);
    DOMWindow* window = frame->domWindow();
    v8::Handle<v8::Value> global = toV8(window, v8::Handle<v8::Object>(), v8Context->GetIsolate());
    ASSERT(global->IsObject());

    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(global), window, isolate);
}

NPObject* ScriptController::windowScriptNPObject()
{
    if (m_windowScriptNPObject)
        return m_windowScriptNPObject;

    if (canExecuteScripts(NotAboutToExecuteScript)) {
        // JavaScript is enabled, so there is a JavaScript window object.
        // Return an NPObject bound to the window object.
        m_windowScriptNPObject = createScriptObject(m_frame, m_isolate);
        _NPN_RegisterObject(m_windowScriptNPObject, 0);
    } else {
        // JavaScript is not enabled, so we cannot bind the NPObject to the
        // JavaScript window object. Instead, we create an NPObject of a
        // different class, one which is not bound to a JavaScript object.
        m_windowScriptNPObject = createNoScriptObject();
    }
    return m_windowScriptNPObject;
}

NPObject* ScriptController::createScriptObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create NPObjects when JavaScript is disabled.
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return createNoScriptObject();

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> v8Context = toV8Context(m_isolate, m_frame, DOMWrapperWorld::mainWorld());
    if (v8Context.IsEmpty())
        return createNoScriptObject();
    v8::Context::Scope scope(v8Context);

    DOMWindow* window = m_frame->domWindow();
    v8::Handle<v8::Value> v8plugin = toV8(plugin, v8::Handle<v8::Object>(), v8Context->GetIsolate());
    if (!v8plugin->IsObject())
        return createNoScriptObject();

    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(v8plugin), window, v8Context->GetIsolate());
}

void ScriptController::clearWindowShell()
{
    double start = currentTime();
    // V8 binding expects ScriptController::clearWindowShell only be called
    // when a frame is loading a new page. This creates a new context for the new page.
    m_windowShell->clearForNavigation();
    for (IsolatedWorldMap::iterator iter = m_isolatedWorlds.begin(); iter != m_isolatedWorlds.end(); ++iter)
        iter->value->clearForNavigation();
    clearScriptObjects();
    blink::Platform::current()->histogramCustomCounts("WebCore.ScriptController.clearWindowShell", (currentTime() - start) * 1000, 0, 10000, 50);
}

void ScriptController::setCaptureCallStackForUncaughtExceptions(bool value)
{
    v8::V8::SetCaptureStackTraceForUncaughtExceptions(value, ScriptCallStack::maxCallStackSizeToCapture, stackTraceOptions);
}

void ScriptController::collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*> >& result)
{
    v8::HandleScope handleScope(m_isolate);
    for (IsolatedWorldMap::iterator it = m_isolatedWorlds.begin(); it != m_isolatedWorlds.end(); ++it) {
        V8WindowShell* isolatedWorldShell = it->value.get();
        SecurityOrigin* origin = isolatedWorldShell->world()->isolatedWorldSecurityOrigin();
        if (!origin)
            continue;
        v8::Local<v8::Context> v8Context = isolatedWorldShell->context();
        if (v8Context.IsEmpty())
            continue;
        ScriptState* scriptState = ScriptState::forContext(v8Context);
        result.append(std::pair<ScriptState*, SecurityOrigin*>(scriptState, origin));
    }
}

bool ScriptController::setContextDebugId(int debugId)
{
    ASSERT(debugId > 0);
    if (!m_windowShell->isContextInitialized())
        return false;
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Context> context = m_windowShell->context();
    return V8PerContextDebugData::setContextDebugData(context, "page", debugId);
}

int ScriptController::contextDebugId(v8::Handle<v8::Context> context)
{
    return V8PerContextDebugData::contextDebugId(context);
}

void ScriptController::updateDocument()
{
    // For an uninitialized main window shell, do not incur the cost of context initialization during FrameLoader::init().
    if ((!m_windowShell->isContextInitialized() || !m_windowShell->isGlobalInitialized()) && m_frame->loader().stateMachine()->creatingInitialEmptyDocument())
        return;

    if (!initializeMainWorld())
        windowShell(DOMWrapperWorld::mainWorld())->updateDocument();
}

void ScriptController::namedItemAdded(HTMLDocument* doc, const AtomicString& name)
{
    windowShell(DOMWrapperWorld::mainWorld())->namedItemAdded(doc, name);
}

void ScriptController::namedItemRemoved(HTMLDocument* doc, const AtomicString& name)
{
    windowShell(DOMWrapperWorld::mainWorld())->namedItemRemoved(doc, name);
}

bool ScriptController::canExecuteScripts(ReasonForCallingCanExecuteScripts reason)
{
    if (m_frame->document() && m_frame->document()->isSandboxed(SandboxScripts)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        if (reason == AboutToExecuteScript)
            m_frame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked script execution in '" + m_frame->document()->url().elidedString() + "' because the document's frame is sandboxed and the 'allow-scripts' permission is not set.");
        return false;
    }

    if (m_frame->document() && m_frame->document()->isViewSource()) {
        ASSERT(m_frame->document()->securityOrigin()->isUnique());
        return true;
    }

    Settings* settings = m_frame->settings();
    const bool allowed = m_frame->loader().client()->allowScript(settings && settings->scriptEnabled());
    if (!allowed && reason == AboutToExecuteScript)
        m_frame->loader().client()->didNotAllowScript();
    return allowed;
}

bool ScriptController::executeScriptIfJavaScriptURL(const KURL& url)
{
    if (!protocolIsJavaScript(url))
        return false;

    if (!m_frame->page()
        || !m_frame->document()->contentSecurityPolicy()->allowJavaScriptURLs(m_frame->document()->url(), eventHandlerPosition().m_line))
        return true;

    // We need to hold onto the LocalFrame here because executing script can
    // destroy the frame.
    RefPtr<LocalFrame> protector(m_frame);
    RefPtr<Document> ownerDocument(m_frame->document());

    const int javascriptSchemeLength = sizeof("javascript:") - 1;

    bool locationChangeBefore = m_frame->navigationScheduler().locationChangePending();

    String decodedURL = decodeURLEscapeSequences(url.string());
    ScriptValue result = evaluateScriptInMainWorld(ScriptSourceCode(decodedURL.substring(javascriptSchemeLength)), NotSharableCrossOrigin, DoNotExecuteScriptWhenScriptsDisabled);

    // If executing script caused this frame to be removed from the page, we
    // don't want to try to replace its document!
    if (!m_frame->page())
        return true;

    String scriptResult;
    if (!result.getString(scriptResult))
        return true;

    // We're still in a frame, so there should be a DocumentLoader.
    ASSERT(m_frame->document()->loader());

    if (!locationChangeBefore && m_frame->navigationScheduler().locationChangePending())
        return true;

    // DocumentWriter::replaceDocument can cause the DocumentLoader to get deref'ed and possible destroyed,
    // so protect it with a RefPtr.
    if (RefPtr<DocumentLoader> loader = m_frame->document()->loader()) {
        UseCounter::count(*m_frame->document(), UseCounter::ReplaceDocumentViaJavaScriptURL);
        loader->replaceDocument(scriptResult, ownerDocument.get());
    }
    return true;
}

void ScriptController::executeScriptInMainWorld(const String& script, ExecuteScriptPolicy policy)
{
    evaluateScriptInMainWorld(ScriptSourceCode(script), NotSharableCrossOrigin, policy);
}

void ScriptController::executeScriptInMainWorld(const ScriptSourceCode& sourceCode, AccessControlStatus corsStatus)
{
    evaluateScriptInMainWorld(sourceCode, corsStatus, DoNotExecuteScriptWhenScriptsDisabled);
}

ScriptValue ScriptController::executeScriptInMainWorldAndReturnValue(const ScriptSourceCode& sourceCode)
{
    return evaluateScriptInMainWorld(sourceCode, NotSharableCrossOrigin, DoNotExecuteScriptWhenScriptsDisabled);
}

ScriptValue ScriptController::evaluateScriptInMainWorld(const ScriptSourceCode& sourceCode, AccessControlStatus corsStatus, ExecuteScriptPolicy policy)
{
    if (policy == DoNotExecuteScriptWhenScriptsDisabled && !canExecuteScripts(AboutToExecuteScript))
        return ScriptValue();

    String sourceURL = sourceCode.url();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> v8Context = toV8Context(m_isolate, m_frame, DOMWrapperWorld::mainWorld());
    if (v8Context.IsEmpty())
        return ScriptValue();

    RefPtr<LocalFrame> protect(m_frame);
    if (m_frame->loader().stateMachine()->isDisplayingInitialEmptyDocument())
        m_frame->loader().didAccessInitialDocument();

    OwnPtr<ScriptSourceCode> maybeProcessedSourceCode =  InspectorInstrumentation::preprocess(m_frame, sourceCode);
    const ScriptSourceCode& sourceCodeToCompile = maybeProcessedSourceCode ? *maybeProcessedSourceCode : sourceCode;

    v8::Local<v8::Value> object = executeScriptAndReturnValue(v8Context, sourceCodeToCompile, corsStatus);
    m_sourceURL = savedSourceURL;

    if (object.IsEmpty())
        return ScriptValue();

    return ScriptValue(object, m_isolate);
}

void ScriptController::executeScriptInIsolatedWorld(int worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup, Vector<ScriptValue>* results)
{
    ASSERT(worldID > 0);

    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Array> v8Results;
    {
        v8::EscapableHandleScope evaluateHandleScope(m_isolate);
        RefPtr<DOMWrapperWorld> world = DOMWrapperWorld::ensureIsolatedWorld(worldID, extensionGroup);
        V8WindowShell* isolatedWorldShell = windowShell(world.get());

        if (!isolatedWorldShell->isContextInitialized())
            return;

        v8::Local<v8::Context> context = isolatedWorldShell->context();
        v8::Context::Scope contextScope(context);
        v8::Local<v8::Array> resultArray = v8::Array::New(m_isolate, sources.size());

        for (size_t i = 0; i < sources.size(); ++i) {
            v8::Local<v8::Value> evaluationResult = executeScriptAndReturnValue(context, sources[i]);
            if (evaluationResult.IsEmpty())
                evaluationResult = v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
            resultArray->Set(i, evaluationResult);
        }

        v8Results = evaluateHandleScope.Escape(resultArray);
    }

    if (results && !v8Results.IsEmpty()) {
        for (size_t i = 0; i < v8Results->Length(); ++i)
            results->append(ScriptValue(v8Results->Get(i), m_isolate));
    }
}

} // namespace WebCore
