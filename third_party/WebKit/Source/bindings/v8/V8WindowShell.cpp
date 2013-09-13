/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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
#include "bindings/v8/V8WindowShell.h"

#include "RuntimeEnabledFeatures.h"
#include "V8Document.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8Window.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/DateExtension.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCForContextDispose.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8Initializer.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/HistogramSupport.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/Assertions.h"
#include "wtf/OwnArrayPtr.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include <algorithm>
#include <utility>
#include <v8-debug.h>
#include <v8.h>

namespace WebCore {

static void checkDocumentWrapper(v8::Handle<v8::Object> wrapper, Document* document)
{
    ASSERT(V8Document::toNative(wrapper) == document);
    ASSERT(!document->isHTMLDocument() || (V8Document::toNative(v8::Handle<v8::Object>::Cast(wrapper->GetPrototype())) == document));
}

static void setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext, int debugId)
{
    V8PerContextDebugData::setContextDebugData(targetContext, "injected", debugId);
}

PassOwnPtr<V8WindowShell> V8WindowShell::create(Frame* frame, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
{
    return adoptPtr(new V8WindowShell(frame, world, isolate));
}

V8WindowShell::V8WindowShell(Frame* frame, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
    : m_frame(frame)
    , m_world(world)
    , m_isolate(isolate)
{
}

void V8WindowShell::disposeContext()
{
    m_perContextData.clear();

    if (m_context.isEmpty())
        return;

    v8::HandleScope handleScope(m_isolate);
    m_frame->loader()->client()->willReleaseScriptContext(m_context.newLocal(m_isolate), m_world->worldId());

    m_context.clear();

    // It's likely that disposing the context has created a lot of
    // garbage. Notify V8 about this so it'll have a chance of cleaning
    // it up when idle.
    bool isMainFrame = m_frame->page() && (m_frame->page()->mainFrame() == m_frame);
    V8GCForContextDispose::instance().notifyContextDisposed(isMainFrame);
}

void V8WindowShell::clearForClose(bool destroyGlobal)
{
    if (destroyGlobal)
        m_global.clear();

    if (m_context.isEmpty())
        return;

    m_document.clear();
    disposeContext();
}

void V8WindowShell::clearForNavigation()
{
    if (m_context.isEmpty())
        return;

    v8::HandleScope handleScope(m_isolate);
    m_document.clear();

    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);
    v8::Context::Scope contextScope(context);

    // Clear the document wrapper cache before turning on access checks on
    // the old DOMWindow wrapper. This way, access to the document wrapper
    // will be protected by the security checks on the DOMWindow wrapper.
    clearDocumentProperty();

    v8::Handle<v8::Object> windowWrapper = m_global.newLocal(m_isolate)->FindInstanceInPrototypeChain(V8Window::GetTemplate(m_isolate, worldTypeInMainThread(m_isolate)));
    ASSERT(!windowWrapper.IsEmpty());
    windowWrapper->TurnOnAccessCheck();
    context->DetachGlobal();
    disposeContext();
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a Frame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
bool V8WindowShell::initializeIfNeeded()
{
    if (!m_context.isEmpty())
        return true;

    v8::HandleScope handleScope(m_isolate);

    V8Initializer::initializeMainThreadIfNeeded(m_isolate);

    createContext();
    if (m_context.isEmpty())
        return false;

    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);

    m_world->setIsolatedWorldField(context);

    bool isMainWorld = m_world->isMainWorld();

    v8::Context::Scope contextScope(context);

    if (m_global.isEmpty()) {
        m_global.set(m_isolate, context->Global());
        if (m_global.isEmpty()) {
            disposeContext();
            return false;
        }
    }

    if (!isMainWorld) {
        V8WindowShell* mainWindow = m_frame->script()->existingWindowShell(mainThreadNormalWorld());
        if (mainWindow && !mainWindow->context().IsEmpty())
            setInjectedScriptContextDebugId(context, m_frame->script()->contextDebugId(mainWindow->context()));
    }

    m_perContextData = V8PerContextData::create(context);
    if (!m_perContextData->init()) {
        disposeContext();
        return false;
    }
    m_perContextData->setActivityLogger(DOMWrapperWorld::activityLogger(m_world->worldId()));
    if (!installDOMWindow()) {
        disposeContext();
        return false;
    }

    if (isMainWorld) {
        updateDocument();
        setSecurityToken();
        if (m_frame->document()) {
            ContentSecurityPolicy* csp = m_frame->document()->contentSecurityPolicy();
            context->AllowCodeGenerationFromStrings(csp->allowEval(0, ContentSecurityPolicy::SuppressReport));
            context->SetErrorMessageForCodeGenerationFromStrings(v8String(csp->evalDisabledErrorMessage(), m_isolate));
        }
    } else {
        // Using the default security token means that the canAccess is always
        // called, which is slow.
        // FIXME: Use tokens where possible. This will mean keeping track of all
        //        created contexts so that they can all be updated when the
        //        document domain
        //        changes.
        context->UseDefaultSecurityToken();

        SecurityOrigin* origin = m_world->isolatedWorldSecurityOrigin();
        if (origin && InspectorInstrumentation::hasFrontends()) {
            ScriptState* scriptState = ScriptState::forContext(v8::Local<v8::Context>::New(m_isolate, context));
            InspectorInstrumentation::didCreateIsolatedContext(m_frame, scriptState, origin);
        }
    }
    m_frame->loader()->client()->didCreateScriptContext(context, m_world->extensionGroup(), m_world->worldId());
    return true;
}

void V8WindowShell::createContext()
{
    // The activeDocumentLoader pointer could be 0 during frame shutdown.
    // FIXME: Can we remove this check?
    if (!m_frame->loader()->activeDocumentLoader())
        return;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Handle<v8::ObjectTemplate> globalTemplate = V8Window::GetShadowObjectTemplate(m_isolate, m_world->isMainWorld() ? MainWorld : IsolatedWorld);
    if (globalTemplate.IsEmpty())
        return;

    double contextCreationStartInSeconds = currentTime();

    // Used to avoid sleep calls in unload handlers.
    ScriptController::registerExtensionIfNeeded(DateExtension::get());

    // Dynamically tell v8 about our extensions now.
    const V8Extensions& extensions = ScriptController::registeredExtensions();
    OwnArrayPtr<const char*> extensionNames = adoptArrayPtr(new const char*[extensions.size()]);
    int index = 0;
    int extensionGroup = m_world->extensionGroup();
    int worldId = m_world->worldId();
    for (size_t i = 0; i < extensions.size(); ++i) {
        // Ensure our date extension is always allowed.
        if (extensions[i] != DateExtension::get()
            && !m_frame->loader()->client()->allowScriptExtension(extensions[i]->name(), extensionGroup, worldId))
            continue;

        extensionNames[index++] = extensions[i]->name();
    }
    v8::ExtensionConfiguration extensionConfiguration(index, extensionNames.get());

    v8::HandleScope handleScope(m_isolate);
    m_context.set(m_isolate, v8::Context::New(m_isolate, &extensionConfiguration, globalTemplate, m_global.newLocal(m_isolate)));

    double contextCreationDurationInMilliseconds = (currentTime() - contextCreationStartInSeconds) * 1000;
    const char* histogramName = "WebCore.V8WindowShell.createContext.MainWorld";
    if (!m_world->isMainWorld())
        histogramName = "WebCore.V8WindowShell.createContext.IsolatedWorld";
    HistogramSupport::histogramCustomCounts(histogramName, contextCreationDurationInMilliseconds, 0, 10000, 50);
}

bool V8WindowShell::installDOMWindow()
{
    DOMWrapperWorld::setInitializingWindow(true);
    DOMWindow* window = m_frame->domWindow();
    v8::Local<v8::Object> windowWrapper = V8ObjectConstructor::newInstance(V8PerContextData::from(m_context.newLocal(m_isolate))->constructorForType(&V8Window::info));
    if (windowWrapper.IsEmpty())
        return false;

    V8Window::installPerContextProperties(windowWrapper, window, m_isolate);

    V8DOMWrapper::setNativeInfo(v8::Handle<v8::Object>::Cast(windowWrapper->GetPrototype()), &V8Window::info, window);

    // Install the windowWrapper as the prototype of the innerGlobalObject.
    // The full structure of the global object is as follows:
    //
    // outerGlobalObject (Empty object, remains after navigation)
    //   -- has prototype --> innerGlobalObject (Holds global variables, changes during navigation)
    //   -- has prototype --> DOMWindow instance
    //   -- has prototype --> Window.prototype
    //   -- has prototype --> Object.prototype
    //
    // Note: Much of this prototype structure is hidden from web content. The
    //       outer, inner, and DOMWindow instance all appear to be the same
    //       JavaScript object.
    //
    v8::Handle<v8::Object> innerGlobalObject = toInnerGlobalObject(m_context.newLocal(m_isolate));
    V8DOMWrapper::setNativeInfo(innerGlobalObject, &V8Window::info, window);
    innerGlobalObject->SetPrototype(windowWrapper);
    V8DOMWrapper::associateObjectWithWrapper<V8Window>(PassRefPtr<DOMWindow>(window), &V8Window::info, windowWrapper, m_isolate, WrapperConfiguration::Dependent);
    DOMWrapperWorld::setInitializingWindow(false);
    return true;
}

void V8WindowShell::updateDocumentWrapper(v8::Handle<v8::Object> wrapper)
{
    ASSERT(m_world->isMainWorld());
    m_document.set(m_isolate, wrapper);
}

void V8WindowShell::updateDocumentProperty()
{
    if (!m_world->isMainWorld())
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::Value> documentWrapper = toV8(m_frame->document(), v8::Handle<v8::Object>(), context->GetIsolate());
    ASSERT(documentWrapper == m_document.newLocal(m_isolate) || m_document.isEmpty());
    if (m_document.isEmpty())
        updateDocumentWrapper(v8::Handle<v8::Object>::Cast(documentWrapper));
    checkDocumentWrapper(m_document.newLocal(m_isolate), m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the DOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentProperty();
        return;
    }
    ASSERT(documentWrapper->IsObject());
    context->Global()->ForceSet(v8::String::NewSymbol("document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));

    // We also stash a reference to the document on the inner global object so that
    // DOMWindow objects we obtain from JavaScript references are guaranteed to have
    // live Document objects.
    toInnerGlobalObject(context)->SetHiddenValue(V8HiddenPropertyName::document(), documentWrapper);
}

void V8WindowShell::clearDocumentProperty()
{
    ASSERT(!m_context.isEmpty());
    if (!m_world->isMainWorld())
        return;
    v8::HandleScope handleScope(m_isolate);
    m_context.newLocal(m_isolate)->Global()->ForceDelete(v8::String::NewSymbol("document"));
}

void V8WindowShell::setSecurityToken()
{
    ASSERT(m_world->isMainWorld());

    Document* document = m_frame->document();

    // Ask the document's SecurityOrigin to generate a security token.
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    SecurityOrigin* origin = document->securityOrigin();
    String token;
    // We stick with an empty token if document.domain was modified or if we
    // are in the initial empty document, so that we can do a full canAccess
    // check in those cases.
    if (!origin->domainWasSetInDOM()
        && !m_frame->loader()->stateMachine()->isDisplayingInitialEmptyDocument())
        token = document->securityOrigin()->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);
    if (token.isEmpty() || token == "null") {
        context->UseDefaultSecurityToken();
        return;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    context->SetSecurityToken(v8::String::NewSymbol(utf8Token.data(), utf8Token.length()));
}

void V8WindowShell::updateDocument()
{
    ASSERT(m_world->isMainWorld());
    if (m_global.isEmpty())
        return;
    if (m_context.isEmpty())
        return;
    updateDocumentProperty();
    updateSecurityOrigin();
}

static v8::Handle<v8::Value> getNamedProperty(HTMLDocument* htmlDocument, const AtomicString& key, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!htmlDocument->hasNamedItem(key.impl()) && !htmlDocument->hasExtraNamedItem(key.impl()))
        return v8Undefined();

    RefPtr<HTMLCollection> items = htmlDocument->documentNamedItems(key);
    if (items->isEmpty())
        return v8Undefined();

    if (items->hasExactlyOneItem()) {
        Node* node = items->item(0);
        Frame* frame = 0;
        if (node->hasTagName(HTMLNames::iframeTag) && (frame = toHTMLIFrameElement(node)->contentFrame()))
            return toV8(frame->domWindow(), creationContext, isolate);
        return toV8(node, creationContext, isolate);
    }
    return toV8(items.release(), creationContext, isolate);
}

static void getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    // FIXME: Consider passing StringImpl directly.
    AtomicString name = toWebCoreAtomicString(property);
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(info.Holder());
    ASSERT(htmlDocument);
    v8::Handle<v8::Value> result = getNamedProperty(htmlDocument, name, info.Holder(), info.GetIsolate());
    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }
    v8::Handle<v8::Value> prototype = info.Holder()->GetPrototype();
    if (prototype->IsObject()) {
        v8SetReturnValue(info, prototype.As<v8::Object>()->Get(property));
        return;
    }
}

void V8WindowShell::namedItemAdded(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (m_context.isEmpty())
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(m_context.newLocal(m_isolate));

    ASSERT(!m_document.isEmpty());
    v8::Handle<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->SetAccessor(v8String(name, m_isolate), getter);
}

void V8WindowShell::namedItemRemoved(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (m_context.isEmpty())
        return;

    if (document->hasNamedItem(name.impl()) || document->hasExtraNamedItem(name.impl()))
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(m_context.newLocal(m_isolate));

    ASSERT(!m_document.isEmpty());
    v8::Handle<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->Delete(v8String(name, m_isolate));
}

void V8WindowShell::updateSecurityOrigin()
{
    ASSERT(m_world->isMainWorld());
    if (m_context.isEmpty())
        return;
    v8::HandleScope handleScope(m_isolate);
    setSecurityToken();
}

} // WebCore
