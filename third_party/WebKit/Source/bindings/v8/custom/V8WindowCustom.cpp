/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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
#include "V8Window.h"

#include "V8HTMLCollection.h"
#include "V8Node.h"
#include "bindings/v8/BindingSecurity.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScheduledAction.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8EventListener.h"
#include "bindings/v8/V8EventListenerList.h"
#include "bindings/v8/V8GCForContextDispose.h"
#include "bindings/v8/V8HiddenValue.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/DOMWindowTimers.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/storage/Storage.h"
#include "platform/PlatformScreen.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

// FIXME: There is a lot of duplication with SetTimeoutOrInterval() in V8WorkerGlobalScopeCustom.cpp.
// We should refactor this.
void windowSetTimeoutImpl(const v8::FunctionCallbackInfo<v8::Value>& info, bool singleShot, ExceptionState& exceptionState)
{
    int argumentCount = info.Length();

    if (argumentCount < 1)
        return;

    DOMWindow* impl = V8Window::toNative(info.Holder());
    if (!impl->frame() || !impl->document()) {
        exceptionState.throwDOMException(InvalidAccessError, "No script context is available in which to execute the script.");
        return;
    }
    v8::Handle<v8::Context> context = toV8Context(info.GetIsolate(), impl->frame(), DOMWrapperWorld::current(info.GetIsolate()));
    if (context.IsEmpty()) {
        exceptionState.throwDOMException(InvalidAccessError, "No script context is available in which to execute the script.");
        return;
    }

    v8::Handle<v8::Value> function = info[0];
    String functionString;
    if (!function->IsFunction()) {
        if (function->IsString()) {
            functionString = toCoreString(function.As<v8::String>());
        } else {
            v8::Handle<v8::String> v8String = function->ToString();

            // Bail out if string conversion failed.
            if (v8String.IsEmpty())
                return;

            functionString = toCoreString(v8String);
        }

        // Don't allow setting timeouts to run empty functions!
        // (Bug 1009597)
        if (!functionString.length())
            return;
    }

    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState))
        return;

    OwnPtr<ScheduledAction> action;
    if (function->IsFunction()) {
        int paramCount = argumentCount >= 2 ? argumentCount - 2 : 0;
        OwnPtr<v8::Local<v8::Value>[]> params;
        if (paramCount > 0) {
            params = adoptArrayPtr(new v8::Local<v8::Value>[paramCount]);
            for (int i = 0; i < paramCount; i++) {
                // parameters must be globalized
                params[i] = info[i+2];
            }
        }

        // params is passed to action, and released in action's destructor
        ASSERT(impl->frame());
        action = adoptPtr(new ScheduledAction(context, v8::Handle<v8::Function>::Cast(function), paramCount, params.get(), info.GetIsolate()));
    } else {
        if (impl->document() && !impl->document()->contentSecurityPolicy()->allowEval()) {
            v8SetReturnValue(info, 0);
            return;
        }
        ASSERT(impl->frame());
        action = adoptPtr(new ScheduledAction(context, functionString, KURL(), info.GetIsolate()));
    }

    int32_t timeout = argumentCount >= 2 ? info[1]->Int32Value() : 0;
    int timerId;
    if (singleShot)
        timerId = DOMWindowTimers::setTimeout(*impl, action.release(), timeout);
    else
        timerId = DOMWindowTimers::setInterval(*impl, action.release(), timeout);

    // Try to do the idle notification before the timeout expires to get better
    // use of any idle time. Aim for the middle of the interval for simplicity.
    if (timeout >= 0) {
        double maximumFireInterval = static_cast<double>(timeout) / 1000 / 2;
        V8GCForContextDispose::instanceTemplate().notifyIdleSooner(maximumFireInterval);
    }

    v8SetReturnValue(info, timerId);
}

void V8Window::eventAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    LocalFrame* frame = V8Window::toNative(info.Holder())->frame();
    ExceptionState exceptionState(ExceptionState::GetterContext, "event", "Window", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), frame, exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    ASSERT(frame);
    v8::Local<v8::Context> context = toV8Context(info.GetIsolate(), frame, DOMWrapperWorld::current(info.GetIsolate()));
    if (context.IsEmpty())
        return;

    v8::Handle<v8::Value> jsEvent = V8HiddenValue::getHiddenValue(info.GetIsolate(), context->Global(), V8HiddenValue::event(info.GetIsolate()));
    if (jsEvent.IsEmpty())
        return;
    v8SetReturnValue(info, jsEvent);
}

void V8Window::eventAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    LocalFrame* frame = V8Window::toNative(info.Holder())->frame();
    ExceptionState exceptionState(ExceptionState::SetterContext, "event", "Window", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), frame, exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    ASSERT(frame);
    v8::Local<v8::Context> context = toV8Context(info.GetIsolate(), frame, DOMWrapperWorld::current(info.GetIsolate()));
    if (context.IsEmpty())
        return;

    V8HiddenValue::setHiddenValue(info.GetIsolate(), context->Global(), V8HiddenValue::event(info.GetIsolate()), value);
}

void V8Window::frameElementAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    DOMWindow* impl = V8Window::toNative(info.Holder());
    ExceptionState exceptionState(ExceptionState::GetterContext, "frame", "Window", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), impl->frameElement(), exceptionState)) {
        v8SetReturnValueNull(info);
        exceptionState.throwIfNeeded();
        return;
    }

    // The wrapper for an <iframe> should get its prototype from the context of the frame it's in, rather than its own frame.
    // So, use its containing document as the creation context when wrapping.
    v8::Handle<v8::Value> creationContext = toV8(&impl->frameElement()->document(), v8::Handle<v8::Object>(), info.GetIsolate());
    RELEASE_ASSERT(!creationContext.IsEmpty());
    v8::Handle<v8::Value> wrapper = toV8(impl->frameElement(), v8::Handle<v8::Object>::Cast(creationContext), info.GetIsolate());
    v8SetReturnValue(info, wrapper);
}

void V8Window::openerAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    DOMWindow* impl = V8Window::toNative(info.Holder());
    ExceptionState exceptionState(ExceptionState::SetterContext, "opener", "Window", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    // Opener can be shadowed if it is in the same domain.
    // Have a special handling of null value to behave
    // like Firefox. See bug http://b/1224887 & http://b/791706.
    if (value->IsNull()) {
        // impl->frame() cannot be null,
        // otherwise, SameOrigin check would have failed.
        ASSERT(impl->frame());
        impl->frame()->loader().setOpener(0);
    }

    // Delete the accessor from this object.
    info.Holder()->Delete(v8AtomicString(info.GetIsolate(), "opener"));

    // Put property on the front (this) object.
    info.This()->Set(v8AtomicString(info.GetIsolate(), "opener"), value);
}

static bool isLegacyTargetOriginDesignation(v8::Handle<v8::Value> value)
{
    if (value->IsString() || value->IsStringObject())
        return true;
    return false;
}


void V8Window::postMessageMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    // None of these need to be RefPtr because info and context are guaranteed
    // to hold on to them.
    DOMWindow* window = V8Window::toNative(info.Holder());
    DOMWindow* source = callingDOMWindow(info.GetIsolate());

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "postMessage", "Window", info.Holder(), info.GetIsolate());

    // If called directly by WebCore we don't have a calling context.
    if (!source) {
        exceptionState.throwTypeError("No active calling context exists.");
        exceptionState.throwIfNeeded();
        return;
    }

    // This function has variable arguments and can be:
    // Per current spec:
    //   postMessage(message, targetOrigin)
    //   postMessage(message, targetOrigin, {sequence of transferrables})
    // Legacy non-standard implementations in webkit allowed:
    //   postMessage(message, {sequence of transferrables}, targetOrigin);
    MessagePortArray portArray;
    ArrayBufferArray arrayBufferArray;
    int targetOriginArgIndex = 1;
    if (info.Length() > 2) {
        int transferablesArgIndex = 2;
        if (isLegacyTargetOriginDesignation(info[2])) {
            targetOriginArgIndex = 2;
            transferablesArgIndex = 1;
        }
        if (!SerializedScriptValue::extractTransferables(info[transferablesArgIndex], transferablesArgIndex, portArray, arrayBufferArray, exceptionState, info.GetIsolate())) {
            exceptionState.throwIfNeeded();
            return;
        }
    }
    TOSTRING_VOID(V8StringResource<WithUndefinedOrNullCheck>, targetOrigin, info[targetOriginArgIndex]);

    RefPtr<SerializedScriptValue> message = SerializedScriptValue::create(info[0], &portArray, &arrayBufferArray, exceptionState, info.GetIsolate());
    if (exceptionState.throwIfNeeded())
        return;

    window->postMessage(message.release(), &portArray, targetOrigin, source, exceptionState);
    exceptionState.throwIfNeeded();
}

// FIXME(fqian): returning string is cheating, and we should
// fix this by calling toString function on the receiver.
// However, V8 implements toString in JavaScript, which requires
// switching context of receiver. I consider it is dangerous.
void V8Window::toStringMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Object> domWrapper = V8Window::findInstanceInPrototypeChain(info.This(), info.GetIsolate());
    if (domWrapper.IsEmpty()) {
        v8SetReturnValue(info, info.This()->ObjectProtoToString());
        return;
    }
    v8SetReturnValue(info, domWrapper->ObjectProtoToString());
}

class DialogHandler {
public:
    explicit DialogHandler(v8::Handle<v8::Value> dialogArguments)
        : m_dialogArguments(dialogArguments)
    {
    }

    void dialogCreated(DOMWindow*, v8::Isolate*);
    v8::Handle<v8::Value> returnValue(v8::Isolate*) const;

private:
    v8::Handle<v8::Value> m_dialogArguments;
    v8::Handle<v8::Context> m_dialogContext;
};

inline void DialogHandler::dialogCreated(DOMWindow* dialogFrame, v8::Isolate* isolate)
{
    // FIXME: It's wrong to use the current world. Instead we should use the world
    // from which the modal dialog was requested.
    m_dialogContext = dialogFrame->frame() ? toV8Context(isolate, dialogFrame->frame(), DOMWrapperWorld::current(isolate)) : v8::Local<v8::Context>();
    if (m_dialogContext.IsEmpty())
        return;
    if (m_dialogArguments.IsEmpty())
        return;
    v8::Context::Scope scope(m_dialogContext);
    m_dialogContext->Global()->Set(v8AtomicString(isolate, "dialogArguments"), m_dialogArguments);
}

inline v8::Handle<v8::Value> DialogHandler::returnValue(v8::Isolate* isolate) const
{
    if (m_dialogContext.IsEmpty())
        return v8::Undefined(isolate);
    v8::Context::Scope scope(m_dialogContext);
    v8::Handle<v8::Value> returnValue = m_dialogContext->Global()->Get(v8AtomicString(isolate, "returnValue"));
    if (returnValue.IsEmpty())
        return v8::Undefined(isolate);
    return returnValue;
}

static void setUpDialog(DOMWindow* dialog, void* handler)
{
    static_cast<DialogHandler*>(handler)->dialogCreated(dialog, v8::Isolate::GetCurrent());
}

void V8Window::showModalDialogMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DOMWindow* impl = V8Window::toNative(info.Holder());
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "showModalDialog", "Window", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    TOSTRING_VOID(V8StringResource<WithUndefinedOrNullCheck>, urlString, info[0]);
    DialogHandler handler(info[1]);
    TOSTRING_VOID(V8StringResource<WithUndefinedOrNullCheck>, dialogFeaturesString, info[2]);

    impl->showModalDialog(urlString, dialogFeaturesString, callingDOMWindow(info.GetIsolate()), enteredDOMWindow(info.GetIsolate()), setUpDialog, &handler);

    v8SetReturnValue(info, handler.returnValue(info.GetIsolate()));
}

void V8Window::openMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DOMWindow* impl = V8Window::toNative(info.Holder());
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "open", "Window", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    TOSTRING_VOID(V8StringResource<WithUndefinedOrNullCheck>, urlString, info[0]);
    AtomicString frameName;
    if (info[1]->IsUndefined() || info[1]->IsNull()) {
        frameName = "_blank";
    } else {
        TOSTRING_VOID(V8StringResource<>, frameNameResource, info[1]);
        frameName = frameNameResource;
    }
    TOSTRING_VOID(V8StringResource<WithUndefinedOrNullCheck>, windowFeaturesString, info[2]);

    RefPtrWillBeRawPtr<DOMWindow> openedWindow = impl->open(urlString, frameName, windowFeaturesString, callingDOMWindow(info.GetIsolate()), enteredDOMWindow(info.GetIsolate()));
    if (!openedWindow)
        return;

    v8SetReturnValueFast(info, openedWindow.release(), impl);
}

void V8Window::namedPropertyGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{

    DOMWindow* window = V8Window::toNative(info.Holder());
    if (!window)
        return;

    LocalFrame* frame = window->frame();
    // window is detached from a frame.
    if (!frame)
        return;

    // Search sub-frames.
    AtomicString propName = toCoreAtomicString(name);
    LocalFrame* child = frame->tree().scopedChild(propName);
    if (child) {
        v8SetReturnValueFast(info, child->domWindow(), window);
        return;
    }

    // Search IDL functions defined in the prototype
    if (!info.Holder()->GetRealNamedProperty(name).IsEmpty())
        return;

    // Search named items in the document.
    Document* doc = frame->document();

    if (doc && doc->isHTMLDocument()) {
        if (toHTMLDocument(doc)->hasNamedItem(propName) || doc->hasElementWithId(propName.impl())) {
            RefPtr<HTMLCollection> items = doc->windowNamedItems(propName);
            if (!items->isEmpty()) {
                if (items->hasExactlyOneItem()) {
                    v8SetReturnValueFast(info, items->item(0), window);
                    return;
                }
                v8SetReturnValueFast(info, items.release(), window);
                return;
            }
        }
    }
}


void V8Window::setTimeoutMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "setTimeout", "Window", info.Holder(), info.GetIsolate());
    windowSetTimeoutImpl(info, true, exceptionState);
    exceptionState.throwIfNeeded();
}


void V8Window::setIntervalMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "setInterval", "Window", info.Holder(), info.GetIsolate());
    windowSetTimeoutImpl(info, false, exceptionState);
    exceptionState.throwIfNeeded();
}

bool V8Window::namedSecurityCheckCustom(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Handle<v8::Object> window = V8Window::findInstanceInPrototypeChain(host, isolate);
    if (window.IsEmpty())
        return false; // the frame is gone.

    DOMWindow* targetWindow = V8Window::toNative(window);

    ASSERT(targetWindow);

    LocalFrame* target = targetWindow->frame();
    if (!target)
        return false;

    // Notify the loader's client if the initial document has been accessed.
    if (target->loader().stateMachine()->isDisplayingInitialEmptyDocument())
        target->loader().didAccessInitialDocument();

    if (key->IsString()) {
        DEFINE_STATIC_LOCAL(const AtomicString, nameOfProtoProperty, ("__proto__", AtomicString::ConstructFromLiteral));

        AtomicString name = toCoreAtomicString(key.As<v8::String>());
        LocalFrame* childFrame = target->tree().scopedChild(name);
        // Notice that we can't call HasRealNamedProperty for ACCESS_HAS
        // because that would generate infinite recursion.
        if (type == v8::ACCESS_HAS && childFrame)
            return true;
        // We need to explicitly compare against nameOfProtoProperty because
        // V8's JSObject::LocalLookup finds __proto__ before
        // interceptors and even when __proto__ isn't a "real named property".
        v8::Handle<v8::String> keyString = key.As<v8::String>();
        if (type == v8::ACCESS_GET
            && childFrame
            && !host->HasRealNamedProperty(keyString)
            && !window->HasRealNamedProperty(keyString)
            && name != nameOfProtoProperty)
            return true;
    }

    return BindingSecurity::shouldAllowAccessToFrame(isolate, target, DoNotReportSecurityError);
}

bool V8Window::indexedSecurityCheckCustom(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Handle<v8::Object> window = V8Window::findInstanceInPrototypeChain(host, isolate);
    if (window.IsEmpty())
        return false;

    DOMWindow* targetWindow = V8Window::toNative(window);

    ASSERT(targetWindow);

    LocalFrame* target = targetWindow->frame();
    if (!target)
        return false;

    // Notify the loader's client if the initial document has been accessed.
    if (target->loader().stateMachine()->isDisplayingInitialEmptyDocument())
        target->loader().didAccessInitialDocument();

    LocalFrame* childFrame =  target->tree().scopedChild(index);

    // Notice that we can't call HasRealNamedProperty for ACCESS_HAS
    // because that would generate infinite recursion.
    if (type == v8::ACCESS_HAS && childFrame)
        return true;
    if (type == v8::ACCESS_GET
        && childFrame
        && !host->HasRealIndexedProperty(index)
        && !window->HasRealIndexedProperty(index))
        return true;

    return BindingSecurity::shouldAllowAccessToFrame(isolate, target, DoNotReportSecurityError);
}

v8::Handle<v8::Value> toV8(DOMWindow* window, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // Notice that we explicitly ignore creationContext because the DOMWindow is its own creationContext.

    if (!window)
        return v8::Null(isolate);
    // Initializes environment of a frame, and return the global object
    // of the frame.
    LocalFrame* frame = window->frame();
    if (!frame)
        return v8Undefined();

    // Special case: Because of executeScriptInIsolatedWorld() one DOMWindow can have
    // multiple contexts and multiple global objects associated with it. When
    // code running in one of those contexts accesses the window object, we
    // want to return the global object associated with that context, not
    // necessarily the first global object associated with that DOMWindow.
    v8::Handle<v8::Object> currentGlobal = isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Object> windowWrapper = V8Window::findInstanceInPrototypeChain(currentGlobal, isolate);
    if (!windowWrapper.IsEmpty()) {
        if (V8Window::toNative(windowWrapper) == window)
            return currentGlobal;
    }

    // Otherwise, return the global object associated with this frame.
    v8::Handle<v8::Context> context = toV8Context(isolate, frame, DOMWrapperWorld::current(isolate));
    if (context.IsEmpty())
        return v8Undefined();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

} // namespace WebCore
