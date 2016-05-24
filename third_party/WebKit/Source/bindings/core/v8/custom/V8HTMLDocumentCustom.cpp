// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HTMLDocument.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8StringResource.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLIFrameElement.h"

#include <cstring>

namespace blink {

namespace {

v8::Local<v8::Value> getNamedProperty(v8::Isolate* isolate, HTMLDocument* htmlDocument, const AtomicString& key, v8::Local<v8::Object> creationContext)
{
    if (!htmlDocument->hasNamedItem(key) && !htmlDocument->hasExtraNamedItem(key))
        return v8Undefined();
    DocumentNameCollection* items = htmlDocument->documentNamedItems(key);
    if (items->isEmpty())
        return v8Undefined();

    // https://html.spec.whatwg.org/multipage/dom.html#dom-document-namedItem-which
    if (items->hasExactlyOneItem()) {
        HTMLElement* element = items->item(0);
        DCHECK(element);
        if (isHTMLIFrameElement(*element)) {
            Frame* frame = toHTMLIFrameElement(*element).contentFrame();
            if (frame)
                return toV8(frame->domWindow(), creationContext, isolate);
        }
        return toV8(element, creationContext, isolate);
    }
    return toV8(items, creationContext, isolate);
}

void propertyGetterCustom(const AtomicString& name, v8::PropertyCallbackInfo<v8::Value> const& info)
{
    HTMLDocument* htmlDocument = V8HTMLDocument::toImpl(info.Holder());
    DCHECK(htmlDocument);

    v8::Local<v8::Value> result = getNamedProperty(info.GetIsolate(), htmlDocument, name, info.Holder());
    if (result.IsEmpty())
        return;

    v8SetReturnValue(info, result);
}

} // namespace

void V8HTMLDocument::indexedPropertyGetterCustom(unsigned index, v8::PropertyCallbackInfo<v8::Value> const& info)
{
    propertyGetterCustom(AtomicString::number(index), info);
}

void V8HTMLDocument::namedPropertyGetterCustom(v8::Local<v8::Name> name, v8::PropertyCallbackInfo<v8::Value> const& info)
{
    v8::String::Utf8Value nameStr(name);

    // [Unforgeable] attributes in an [OverrideBuiltin] interface must be looked
    // up before any named properties
    // (http://heycam.github.io/webidl/#dfn-named-property-visibility).
    // So we return immediately before looking up named propertiees.
    // TODO(peria): Support the behavior correctly.
    if (std::strcmp(*nameStr, "location") == 0)
        return;

    propertyGetterCustom(AtomicString(*nameStr), info);
}

} // namespace blink
