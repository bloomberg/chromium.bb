/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8HTMLAllCollection.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8NodeList.h"
#include "core/dom/StaticNodeList.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAllCollection.h"

namespace blink {

template <class CallbackInfo>
static v8::Local<v8::Value> GetNamedItems(HTMLAllCollection* collection,
                                          AtomicString name,
                                          const CallbackInfo& info) {
  HeapVector<Member<Element>> named_items;
  collection->NamedItems(name, named_items);

  if (!named_items.size())
    return V8Undefined();

  if (named_items.size() == 1)
    return ToV8(named_items.at(0).Release(), info.Holder(), info.GetIsolate());

  // FIXME: HTML5 specification says this should be a HTMLCollection.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlallcollection
  return ToV8(StaticElementList::Adopt(named_items), info.Holder(),
              info.GetIsolate());
}

template <class CallbackInfo>
static v8::Local<v8::Value> GetItem(
    HTMLAllCollection* collection,
    v8::Local<v8::Value> argument,
    const CallbackInfo& info,
    WebFeature named_feature,
    WebFeature indexed_feature,
    WebFeature indexed_with_non_number_feature) {
  v8::Local<v8::Uint32> index;
  if (!argument->ToArrayIndex(info.GetIsolate()->GetCurrentContext())
           .ToLocal(&index)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      named_feature);
    TOSTRING_DEFAULT(V8StringResource<>, name, argument,
                     v8::Undefined(info.GetIsolate()));
    v8::Local<v8::Value> result = GetNamedItems(collection, name, info);

    if (result.IsEmpty())
      return v8::Undefined(info.GetIsolate());

    return result;
  }

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    indexed_feature);
  if (!argument->IsNumber()) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      indexed_with_non_number_feature);
  }
  Element* result = collection->item(index->Value());
  return ToV8(result, info.Holder(), info.GetIsolate());
}

void V8HTMLAllCollection::itemMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kDocumentAllItemNoArguments);
    return;
  }

  HTMLAllCollection* impl = V8HTMLAllCollection::toImpl(info.Holder());
  V8SetReturnValue(
      info, GetItem(impl, info[0], info, WebFeature::kDocumentAllItemNamed,
                    WebFeature::kDocumentAllItemIndexed,
                    WebFeature::kDocumentAllItemIndexedWithNonNumber));
}

void V8HTMLAllCollection::legacyCallCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_V8HTMLAllCollection_legacyCallCustom");
  if (info.Length() < 1) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kDocumentAllLegacyCallNoArguments);
    return;
  }

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kDocumentAllLegacyCall);

  HTMLAllCollection* impl = V8HTMLAllCollection::toImpl(info.Holder());

  if (info.Length() == 1) {
    V8SetReturnValue(
        info,
        GetItem(impl, info[0], info, WebFeature::kDocumentAllLegacyCallNamed,
                WebFeature::kDocumentAllLegacyCallIndexed,
                WebFeature::kDocumentAllLegacyCallIndexedWithNonNumber));
    return;
  }

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kDocumentAllLegacyCallTwoArguments);

  // If there is a second argument it is the index of the item we want.
  TOSTRING_VOID(V8StringResource<>, name, info[0]);
  v8::Local<v8::Uint32> index;
  if (!info[1]
           ->ToArrayIndex(info.GetIsolate()->GetCurrentContext())
           .ToLocal(&index))
    return;

  if (Node* node = impl->NamedItemWithIndex(name, index->Value())) {
    V8SetReturnValueFast(info, node, impl);
    return;
  }
}

}  // namespace blink
