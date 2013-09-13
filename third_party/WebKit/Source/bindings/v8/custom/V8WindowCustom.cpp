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
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScheduledAction.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8EventListener.h"
#include "bindings/v8/V8EventListenerList.h"
#include "bindings/v8/V8GCForContextDispose.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8Utilities.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Chrome.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMTimer.h"
#include "core/page/DOMWindow.h"
#include "core/page/DOMWindowTimers.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Location.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/PlatformScreen.h"
#include "core/platform/graphics/MediaPlayer.h"
#include "core/storage/Storage.h"
#include "core/workers/SharedWorkerRepository.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/OwnArrayPtr.h"

namespace WebCore {

// FIXME: There is a lot of duplication with SetTimeoutOrInterval() in V8WorkerGlobalScopeCustom.cpp.
// We should refactor this.
void WindowSetTimeoutImpl(const v8::FunctionCallbackInfo<v8::Value>& args, bool singleShot, ExceptionState& es)
{
    int argumentCount = args.Length();

    if (argumentCount < 1)
        return;

    DOMWindow* imp = V8Window::toNative(args.Holder());
    ScriptExecutionContext* scriptContext = static_cast<ScriptExecutionContext*>(imp->document());

    if (!scriptContext) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    v8::Handle<v8::Value> function = args[0];
    String functionString;
    if (!function->IsFunction()) {
        if (function->IsString()) {
            functionString = toWebCoreString(function);
        } else {
            v8::Handle<v8::Value> v8String = function->ToString();

            // Bail out if string conversion failed.
            if (v8String.IsEmpty())
                return;

            functionString = toWebCoreString(v8String);
        }

        // Don't allow setting timeouts to run empty functions!
        // (Bug 1009597)
        if (!functionString.length())
            return;
    }

    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame(), es))
        return;

    OwnPtr<ScheduledAction> action;
    if (function->IsFunction()) {
        int paramCount = argumentCount >= 2 ? argumentCount - 2 : 0;
        OwnArrayPtr<v8::Local<v8::Value> > params;
        if (paramCount > 0) {
            params = adoptArrayPtr(new v8::Local<v8::Value>[paramCount]);
            for (int i = 0; i < paramCount; i++) {
                // parameters must be globalized
                params[i] = args[i+2];
            }
        }

        // params is passed to action, and released in action's destructor
        ASSERT(imp->frame());
        action = adoptPtr(new ScheduledAction(imp->frame()->script()->currentWorldContext(), v8::Handle<v8::Function>::Cast(function), paramCount, params.get(), args.GetIsolate()));
    } else {
        if (imp->document() && !imp->document()->contentSecurityPolicy()->allowEval()) {
            v8SetReturnValue(args, 0);
            return;
        }
        ASSERT(imp->frame());
        action = adoptPtr(new ScheduledAction(imp->frame()->script()->currentWorldContext(), functionString, KURL(), args.GetIsolate()));
    }

    int32_t timeout = argumentCount >= 2 ? args[1]->Int32Value() : 0;
    int timerId;
    if (singleShot)
        timerId = DOMWindowTimers::setTimeout(imp, action.release(), timeout);
    else
        timerId = DOMWindowTimers::setInterval(imp, action.release(), timeout);

    // Try to do the idle notification before the timeout expires to get better
    // use of any idle time. Aim for the middle of the interval for simplicity.
    if (timeout >= 0) {
        double maximumFireInterval = static_cast<double>(timeout) / 1000 / 2;
        V8GCForContextDispose::instance().notifyIdleSooner(maximumFireInterval);
    }

    v8SetReturnValue(args, timerId);
}

void V8Window::eventAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(V8Window::GetTemplate(info.GetIsolate(), worldTypeInMainThread(info.GetIsolate())));
    if (holder.IsEmpty())
        return;

    Frame* frame = V8Window::toNative(holder)->frame();
    ExceptionState es(info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(frame, es)) {
        es.throwIfNeeded();
        return;
    }

    ASSERT(frame);
    v8::Local<v8::Context> context = frame->script()->currentWorldContext();
    if (context.IsEmpty())
        return;

    v8::Handle<v8::String> eventSymbol = V8HiddenPropertyName::event();
    v8::Handle<v8::Value> jsEvent = context->Global()->GetHiddenValue(eventSymbol);
    if (jsEvent.IsEmpty())
        return;
    v8SetReturnValue(info, jsEvent);
}

void V8Window::eventAttributeSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(V8Window::GetTemplate(info.GetIsolate(), worldTypeInMainThread(info.GetIsolate())));
    if (holder.IsEmpty())
        return;

    Frame* frame = V8Window::toNative(holder)->frame();
    ExceptionState es(info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(frame, es)) {
        es.throwIfNeeded();
        return;
    }

    ASSERT(frame);
    v8::Local<v8::Context> context = frame->script()->currentWorldContext();
    if (context.IsEmpty())
        return;

    v8::Handle<v8::String> eventSymbol = V8HiddenPropertyName::event();
    context->Global()->SetHiddenValue(eventSymbol, value);
}

void V8Window::locationAttributeSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    DOMWindow* imp = V8Window::toNative(info.Holder());

    DOMWindow* active = activeDOMWindow();
    if (!active)
        return;

    DOMWindow* first = firstDOMWindow();
    if (!first)
        return;

    if (Location* location = imp->location()) {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, href, value);
        location->setHref(active, first, href);
    }
}

void V8Window::openerAttributeSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    DOMWindow* imp = V8Window::toNative(info.Holder());
    ExceptionState es(info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame(), es)) {
        es.throwIfNeeded();
        return;
    }

    // Opener can be shadowed if it is in the same domain.
    // Have a special handling of null value to behave
    // like Firefox. See bug http://b/1224887 & http://b/791706.
    if (value->IsNull()) {
        // imp->frame() cannot be null,
        // otherwise, SameOrigin check would have failed.
        ASSERT(imp->frame());
        imp->frame()->loader()->setOpener(0);
    }

    // Delete the accessor from this object.
    info.Holder()->Delete(name);

    // Put property on the front (this) object.
    info.This()->Set(name, value);
}

static bool isLegacyTargetOriginDesignation(v8::Handle<v8::Value> value)
{
    if (value->IsString() || value->IsStringObject())
        return true;
    return false;
}


void V8Window::postMessageMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    // None of these need to be RefPtr because args and context are guaranteed
    // to hold on to them.
    DOMWindow* window = V8Window::toNative(args.Holder());
    DOMWindow* source = activeDOMWindow();

    // If called directly by WebCore we don't have a calling context.
    if (!source) {
        throwTypeError(args.GetIsolate());
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
    if (args.Length() > 2) {
        int transferablesArgIndex = 2;
        if (isLegacyTargetOriginDesignation(args[2])) {
            targetOriginArgIndex = 2;
            transferablesArgIndex = 1;
        }
        if (!extractTransferables(args[transferablesArgIndex], portArray, arrayBufferArray, args.GetIsolate()))
            return;
    }
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithUndefinedOrNullCheck>, targetOrigin, args[targetOriginArgIndex]);

    bool didThrow = false;
    RefPtr<SerializedScriptValue> message =
        SerializedScriptValue::create(args[0], &portArray, &arrayBufferArray, didThrow, args.GetIsolate());
    if (didThrow)
        return;

    ExceptionState es(args.GetIsolate());
    window->postMessage(message.release(), &portArray, targetOrigin, source, es);
    es.throwIfNeeded();
}

// FIXME(fqian): returning string is cheating, and we should
// fix this by calling toString function on the receiver.
// However, V8 implements toString in JavaScript, which requires
// switching context of receiver. I consider it is dangerous.
void V8Window::toStringMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> domWrapper = args.This()->FindInstanceInPrototypeChain(V8Window::GetTemplate(args.GetIsolate(), worldTypeInMainThread(args.GetIsolate())));
    if (domWrapper.IsEmpty()) {
        v8SetReturnValue(args, args.This()->ObjectProtoToString());
        return;
    }
    v8SetReturnValue(args, domWrapper->ObjectProtoToString());
}

class DialogHandler {
public:
    explicit DialogHandler(v8::Handle<v8::Value> dialogArguments)
        : m_dialogArguments(dialogArguments)
    {
    }

    void dialogCreated(DOMWindow*);
    v8::Handle<v8::Value> returnValue(v8::Isolate*) const;

private:
    v8::Handle<v8::Value> m_dialogArguments;
    v8::Handle<v8::Context> m_dialogContext;
};

inline void DialogHandler::dialogCreated(DOMWindow* dialogFrame)
{
    m_dialogContext = dialogFrame->frame() ? dialogFrame->frame()->script()->currentWorldContext() : v8::Local<v8::Context>();
    if (m_dialogContext.IsEmpty())
        return;
    if (m_dialogArguments.IsEmpty())
        return;
    v8::Context::Scope scope(m_dialogContext);
    m_dialogContext->Global()->Set(v8::String::NewSymbol("dialogArguments"), m_dialogArguments);
}

inline v8::Handle<v8::Value> DialogHandler::returnValue(v8::Isolate* isolate) const
{
    if (m_dialogContext.IsEmpty())
        return v8::Undefined(isolate);
    v8::Context::Scope scope(m_dialogContext);
    v8::Handle<v8::Value> returnValue = m_dialogContext->Global()->Get(v8::String::NewSymbol("returnValue"));
    if (returnValue.IsEmpty())
        return v8::Undefined(isolate);
    return returnValue;
}

static void setUpDialog(DOMWindow* dialog, void* handler)
{
    static_cast<DialogHandler*>(handler)->dialogCreated(dialog);
}

void V8Window::showModalDialogMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    DOMWindow* impl = V8Window::toNative(args.Holder());
    ExceptionState es(args.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(impl->frame(), es)) {
        es.throwIfNeeded();
        return;
    }

    // FIXME: Handle exceptions properly.
    String urlString = toWebCoreStringWithUndefinedOrNullCheck(args[0]);
    DialogHandler handler(args[1]);
    String dialogFeaturesString = toWebCoreStringWithUndefinedOrNullCheck(args[2]);

    impl->showModalDialog(urlString, dialogFeaturesString, activeDOMWindow(), firstDOMWindow(), setUpDialog, &handler);

    v8SetReturnValue(args, handler.returnValue(args.GetIsolate()));
}

void V8Window::openMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    DOMWindow* impl = V8Window::toNative(args.Holder());
    ExceptionState es(args.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(impl->frame(), es)) {
        es.throwIfNeeded();
        return;
    }

    // FIXME: Handle exceptions properly.
    String urlString = toWebCoreStringWithUndefinedOrNullCheck(args[0]);
    AtomicString frameName = (args[1]->IsUndefined() || args[1]->IsNull()) ? "_blank" : toWebCoreAtomicString(args[1]);
    String windowFeaturesString = toWebCoreStringWithUndefinedOrNullCheck(args[2]);

    RefPtr<DOMWindow> openedWindow = impl->open(urlString, frameName, windowFeaturesString, activeDOMWindow(), firstDOMWindow());
    if (!openedWindow)
        return;

    v8SetReturnValueFast(args, openedWindow.release(), impl);
}

void V8Window::namedPropertyGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{

    DOMWindow* window = V8Window::toNative(info.Holder());
    if (!window)
        return;

    Frame* frame = window->frame();
    // window is detached from a frame.
    if (!frame)
        return;

    // Search sub-frames.
    AtomicString propName = toWebCoreAtomicString(name);
    Frame* child = frame->tree()->scopedChild(propName);
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
        if (toHTMLDocument(doc)->hasNamedItem(propName.impl()) || doc->hasElementWithId(propName.impl())) {
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


void V8Window::setTimeoutMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    ExceptionState es(args.GetIsolate());
    WindowSetTimeoutImpl(args, true, es);
    es.throwIfNeeded();
}


void V8Window::setIntervalMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    ExceptionState es(args.GetIsolate());
    WindowSetTimeoutImpl(args, false, es);
    es.throwIfNeeded();
}

bool V8Window::namedSecurityCheckCustom(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Handle<v8::Object> window = host->FindInstanceInPrototypeChain(V8Window::GetTemplate(isolate, worldTypeInMainThread(isolate)));
    if (window.IsEmpty())
        return false; // the frame is gone.

    DOMWindow* targetWindow = V8Window::toNative(window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;

    // Notify the loader's client if the initial document has been accessed.
    if (target->loader()->stateMachine()->isDisplayingInitialEmptyDocument())
        target->loader()->didAccessInitialDocument();

    if (key->IsString()) {
        DEFINE_STATIC_LOCAL(AtomicString, nameOfProtoProperty, ("__proto__", AtomicString::ConstructFromLiteral));

        String name = toWebCoreString(key);
        Frame* childFrame = target->tree()->scopedChild(name);
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

    return BindingSecurity::shouldAllowAccessToFrame(target, DoNotReportSecurityError);
}

bool V8Window::indexedSecurityCheckCustom(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Handle<v8::Object> window = host->FindInstanceInPrototypeChain(V8Window::GetTemplate(isolate, worldTypeInMainThread(isolate)));
    if (window.IsEmpty())
        return false;

    DOMWindow* targetWindow = V8Window::toNative(window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;

    // Notify the loader's client if the initial document has been accessed.
    if (target->loader()->stateMachine()->isDisplayingInitialEmptyDocument())
        target->loader()->didAccessInitialDocument();

    Frame* childFrame =  target->tree()->scopedChild(index);

    // Notice that we can't call HasRealNamedProperty for ACCESS_HAS
    // because that would generate infinite recursion.
    if (type == v8::ACCESS_HAS && childFrame)
        return true;
    if (type == v8::ACCESS_GET
        && childFrame
        && !host->HasRealIndexedProperty(index)
        && !window->HasRealIndexedProperty(index))
        return true;

    return BindingSecurity::shouldAllowAccessToFrame(target, DoNotReportSecurityError);
}

v8::Handle<v8::Value> toV8(DOMWindow* window, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // Notice that we explicitly ignore creationContext because the DOMWindow is its own creationContext.

    if (!window)
        return v8NullWithCheck(isolate);
    // Initializes environment of a frame, and return the global object
    // of the frame.
    Frame* frame = window->frame();
    if (!frame)
        return v8Undefined();

    // Special case: Because of executeScriptInIsolatedWorld() one DOMWindow can have
    // multiple contexts and multiple global objects associated with it. When
    // code running in one of those contexts accesses the window object, we
    // want to return the global object associated with that context, not
    // necessarily the first global object associated with that DOMWindow.
    v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
    v8::Handle<v8::Object> currentGlobal = currentContext->Global();
    v8::Handle<v8::Object> windowWrapper = currentGlobal->FindInstanceInPrototypeChain(V8Window::GetTemplate(isolate, worldTypeInMainThread(isolate)));
    if (!windowWrapper.IsEmpty()) {
        if (V8Window::toNative(windowWrapper) == window)
            return currentGlobal;
    }

    // Otherwise, return the global object associated with this frame.
    v8::Handle<v8::Context> context = frame->script()->currentWorldContext();
    if (context.IsEmpty())
        return v8Undefined();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

} // namespace WebCore
