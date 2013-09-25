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

#include "config.h"
#include "V8HTMLOptionsCollection.h"

#include "V8HTMLOptionElement.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NamedNodesCollection.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/HTMLSelectElement.h"

namespace WebCore {

template<typename CallbackInfo>
static void getNamedItems(HTMLOptionsCollection* collection, const AtomicString& name, const CallbackInfo& callbackInfo)
{
    Vector<RefPtr<Node> > namedItems;
    collection->namedItems(name, namedItems);

    if (!namedItems.size()) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }

    if (namedItems.size() == 1) {
        v8SetReturnValueFast(callbackInfo, namedItems.at(0).release(), collection);
        return;
    }

    v8SetReturnValueFast(callbackInfo, NamedNodesCollection::create(namedItems), collection);
}

void V8HTMLOptionsCollection::namedItemMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, name, args[0]);
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(args.Holder());
    getNamedItems(imp, name, args);
}

void V8HTMLOptionsCollection::addMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (!V8HTMLOptionElement::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        setDOMException(TypeMismatchError, args.GetIsolate());
        return;
    }
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(args.Holder());
    HTMLOptionElement* option = V8HTMLOptionElement::toNative(v8::Handle<v8::Object>(v8::Handle<v8::Object>::Cast(args[0])));

    ExceptionState es(args.GetIsolate());
    if (args.Length() < 2)
        imp->add(option, es);
    else {
        bool ok;
        V8TRYCATCH_VOID(int, index, toInt32(args[1], ok));
        if (!ok)
            es.throwUninformativeAndGenericDOMException(TypeMismatchError);
        else
            imp->add(option, index, es);
    }

    es.throwIfNeeded();
}

void V8HTMLOptionsCollection::lengthAttributeSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(info.Holder());
    double v = value->NumberValue();
    unsigned newLength = 0;
    ExceptionState es(info.GetIsolate());
    if (!std::isnan(v) && !std::isinf(v)) {
        if (v < 0.0)
            es.throwUninformativeAndGenericDOMException(IndexSizeError);
        else if (v > static_cast<double>(UINT_MAX))
            newLength = UINT_MAX;
        else
            newLength = static_cast<unsigned>(v);
    }

    if (es.throwIfNeeded())
        return;

    imp->setLength(newLength, es);
}

} // namespace WebCore
