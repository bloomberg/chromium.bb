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

#include "bindings/core/v8/V8Window.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8EventListener.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8Node.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Location.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/messaging/MessagePort.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "platform/LayoutTestSupport.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Assertions.h"

namespace blink {

void V8Window::locationAttributeGetterCustom(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Object> holder = info.Holder();

  DOMWindow* window = V8Window::ToImpl(holder);
  Location* location = window->location();
  DCHECK(location);

  // If we have already created a wrapper object in this world, returns it.
  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), location))
    return;

  v8::Local<v8::Value> wrapper;

  // Note that this check is gated on whether or not |window| is remote, not
  // whether or not |window| is cross-origin. If |window| is local, the
  // |location| property must always return the same wrapper, even if the
  // cross-origin status changes by changing properties like |document.domain|.
  if (window->IsRemoteDOMWindow()) {
    DOMWrapperWorld& world = DOMWrapperWorld::Current(isolate);
    const auto* wrapper_type_info = location->GetWrapperTypeInfo();
    v8::Local<v8::Object> new_wrapper =
        wrapper_type_info->domTemplate(isolate, world)
            ->NewRemoteInstance()
            .ToLocalChecked();

    DCHECK(!DOMDataStore::ContainsWrapper(location, isolate));
    wrapper = V8DOMWrapper::AssociateObjectWithWrapper(
        isolate, location, wrapper_type_info, new_wrapper);
  } else {
    wrapper = ToV8(location, holder, isolate);
  }

  V8SetReturnValue(info, wrapper);
}

void V8Window::eventAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = ToLocalDOMWindow(V8Window::ToImpl(info.Holder()));
  v8::Isolate* isolate = info.GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kGetterContext,
                                 "Window", "event");
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(isolate), impl,
                                            exception_state)) {
    return;
  }

  v8::Local<v8::Value> js_event =
      V8PrivateProperty::GetGlobalEvent(isolate).GetOrUndefined(info.Holder());

  // Track usage of window.event when the event's target is inside V0 shadow
  // tree.
  // TODO(yukishiino): Make window.event [Replaceable] and simplify the
  // following IsWrapper/ToImplWithTypeCheck hack.
  if (V8DOMWrapper::IsWrapper(isolate, js_event)) {
    if (Event* event = V8Event::ToImplWithTypeCheck(isolate, js_event)) {
      if (event->target()) {
        Node* target_node = event->target()->ToNode();
        if (target_node && target_node->IsInV0ShadowTree()) {
          UseCounter::Count(CurrentExecutionContext(isolate),
                            WebFeature::kWindowEventInV0ShadowTree);
        }
      }
    }
  }
  V8SetReturnValue(info, js_event);
}

void V8Window::eventAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = ToLocalDOMWindow(V8Window::ToImpl(info.Holder()));
  v8::Isolate* isolate = info.GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kSetterContext,
                                 "Window", "event");
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(isolate), impl,
                                            exception_state)) {
    return;
  }

  V8PrivateProperty::GetGlobalEvent(isolate).Set(info.Holder(), value);
}

void V8Window::frameElementAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = ToLocalDOMWindow(V8Window::ToImpl(info.Holder()));
  Element* frameElement = impl->frameElement();

  if (!BindingSecurity::ShouldAllowAccessTo(
          CurrentDOMWindow(info.GetIsolate()), frameElement,
          BindingSecurity::ErrorReportOption::kDoNotReport)) {
    V8SetReturnValueNull(info);
    return;
  }

  // The wrapper for an <iframe> should get its prototype from the context of
  // the frame it's in, rather than its own frame.
  // So, use its containing document as the creation context when wrapping.
  v8::Local<v8::Value> creation_context =
      ToV8(frameElement->GetDocument().domWindow(), info.Holder(),
           info.GetIsolate());
  CHECK(!creation_context.IsEmpty());
  v8::Local<v8::Value> wrapper =
      ToV8(frameElement, v8::Local<v8::Object>::Cast(creation_context),
           info.GetIsolate());
  V8SetReturnValue(info, wrapper);
}

void V8Window::openerAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  DOMWindow* impl = V8Window::ToImpl(info.Holder());
  if (!impl->GetFrame())
    return;

  // Opener can be shadowed if it is in the same domain.
  // Have a special handling of null value to behave
  // like Firefox. See bug http://b/1224887 & http://b/791706.
  if (value->IsNull()) {
    // impl->frame() has to be a non-null LocalFrame.  Otherwise, the
    // same-origin check would have failed.
    DCHECK(impl->GetFrame());
    ToLocalFrame(impl->GetFrame())->Loader().SetOpener(nullptr);
  }

  // Delete the accessor from the inner object.
  if (info.Holder()
          ->Delete(isolate->GetCurrentContext(),
                   V8AtomicString(isolate, "opener"))
          .IsNothing()) {
    return;
  }

  // Put property on the inner object.
  if (info.Holder()->IsObject()) {
    v8::Maybe<bool> unused =
        v8::Local<v8::Object>::Cast(info.Holder())
            ->Set(isolate->GetCurrentContext(),
                  V8AtomicString(isolate, "opener"), value);
    ALLOW_UNUSED_LOCAL(unused);
  }
}

void V8Window::postMessageMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");
  if (UNLIKELY(info.Length() < 2)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  // None of these need to be RefPtr because info and context are guaranteed
  // to hold on to them.
  DOMWindow* window = V8Window::ToImpl(info.Holder());
  // TODO(yukishiino): The HTML spec specifies that we should use the
  // Incumbent Realm instead of the Current Realm, but currently we don't have
  // a way to retrieve the Incumbent Realm.  See also:
  // https://html.spec.whatwg.org/multipage/comms.html#dom-window-postmessage
  LocalDOMWindow* source = CurrentDOMWindow(info.GetIsolate());

  DCHECK(window);
  UseCounter::Count(source->GetFrame(), WebFeature::kWindowPostMessage);

  // If called directly by WebCore we don't have a calling context.
  if (!source) {
    exception_state.ThrowTypeError("No active calling context exists.");
    return;
  }

  // This function has variable arguments and can be:
  //   postMessage(message, targetOrigin)
  //   postMessage(message, targetOrigin, {sequence of transferrables})
  // TODO(foolip): Type checking of the arguments should happen in order, so
  // that e.g. postMessage({}, { toString: () => { throw Error(); } }, 0)
  // throws the Error from toString, not the TypeError for argument 3.
  Transferables transferables;
  const int kTargetOriginArgIndex = 1;
  if (info.Length() > 2) {
    const int kTransferablesArgIndex = 2;
    if (!SerializedScriptValue::ExtractTransferables(
            info.GetIsolate(), info[kTransferablesArgIndex],
            kTransferablesArgIndex, transferables, exception_state)) {
      return;
    }
  }
  // TODO(foolip): targetOrigin should be a USVString in IDL and treated as
  // such here, without TreatNullAndUndefinedAsNullString.
  TOSTRING_VOID(V8StringResource<kTreatNullAndUndefinedAsNullString>,
                target_origin, info[kTargetOriginArgIndex]);

  SerializedScriptValue::SerializeOptions options;
  options.transferables = &transferables;
  scoped_refptr<SerializedScriptValue> message =
      SerializedScriptValue::Serialize(info.GetIsolate(), info[0], options,
                                       exception_state);
  if (exception_state.HadException())
    return;

  message->UnregisterMemoryAllocatedWithCurrentScriptContext();
  window->postMessage(std::move(message), transferables.message_ports,
                      target_origin, source, exception_state);
}

void V8Window::namedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DOMWindow* window = V8Window::ToImpl(info.Holder());
  if (!window)
    return;

  Frame* frame = window->GetFrame();
  // window is detached from a frame.
  if (!frame)
    return;

  // Note that named access on WindowProxy is allowed in the cross-origin case.
  // 7.4.5 [[GetOwnProperty]] (P), step 6.
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getownproperty
  //
  // 7.3.3 Named access on the Window object
  // The document-tree child browsing context name property set
  // https://html.spec.whatwg.org/multipage/browsers.html#document-tree-child-browsing-context-name-property-set
  Frame* child = frame->Tree().ScopedChild(name);
  if (child) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kNamedAccessOnWindow_ChildBrowsingContext);

    // step 3. Remove each browsing context from childBrowsingContexts whose
    // active document's origin is not same origin with activeDocument's origin
    // and whose browsing context name does not match the name of its browsing
    // context container's name content attribute value.
    if (BindingSecurity::ShouldAllowNamedAccessTo(window, child->DomWindow()) ||
        name == child->Owner()->BrowsingContextContainerName()) {
      V8SetReturnValueFast(info, child->DomWindow(), window);
      return;
    }

    UseCounter::Count(
        CurrentExecutionContext(info.GetIsolate()),
        WebFeature::
            kNamedAccessOnWindow_ChildBrowsingContext_CrossOriginNameMismatch);
    // In addition to the above spec'ed case, we return the child window
    // regardless of step 3 due to crbug.com/701489 for the time being.
    // TODO(yukishiino): Makes iframe.name update the browsing context name
    // appropriately and makes the new name available in the named access on
    // window.  Then, removes the following two lines.
    V8SetReturnValueFast(info, child->DomWindow(), window);
    return;
  }

  // This is a cross-origin interceptor. Check that the caller has access to the
  // named results.
  if (!BindingSecurity::ShouldAllowAccessTo(
          CurrentDOMWindow(info.GetIsolate()), window,
          BindingSecurity::ErrorReportOption::kDoNotReport)) {
    // HTML 7.2.3.3 CrossOriginGetOwnPropertyHelper ( O, P )
    // https://html.spec.whatwg.org/multipage/browsers.html#crossorigingetownpropertyhelper-(-o,-p-)
    // step 3. If P is "then", @@toStringTag, @@hasInstance, or
    //   @@isConcatSpreadable, then return PropertyDescriptor{ [[Value]]:
    //   undefined, [[Writable]]: false, [[Enumerable]]: false,
    //   [[Configurable]]: true }.
    if (name == "then") {
      V8SetReturnValueFast(info, v8::Undefined(info.GetIsolate()), window);
      return;
    }

    BindingSecurity::FailedAccessCheckFor(
        info.GetIsolate(), window->GetWrapperTypeInfo(), info.Holder());
    return;
  }

  // Search named items in the document.
  Document* doc = ToLocalFrame(frame)->GetDocument();
  if (!doc || !doc->IsHTMLDocument())
    return;

  bool has_named_item = ToHTMLDocument(doc)->HasNamedItem(name);
  bool has_id_item = doc->HasElementWithId(name);

  if (!has_named_item && !has_id_item)
    return;

  if (!has_named_item && has_id_item &&
      !doc->ContainsMultipleElementsWithId(name)) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedVariableAccessed);
    V8SetReturnValueFast(info, doc->getElementById(name), window);
    return;
  }

  HTMLCollection* items = doc->WindowNamedItems(name);
  if (!items->IsEmpty()) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedVariableAccessed);

    // TODO(esprehn): Firefox doesn't return an HTMLCollection here if there's
    // multiple with the same name, but Chrome and Safari does. What's the
    // right behavior?
    if (items->HasExactlyOneItem()) {
      V8SetReturnValueFast(info, items->item(0), window);
      return;
    }
    V8SetReturnValueFast(info, items, window);
    return;
  }
}

}  // namespace blink
