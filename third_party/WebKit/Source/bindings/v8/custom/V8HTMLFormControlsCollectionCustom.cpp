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
#include "V8HTMLFormControlsCollection.h"

#include "V8Node.h"
#include "V8RadioNodeList.h"
#include "bindings/v8/V8Binding.h"
#include "core/html/HTMLCollection.h"
#include "core/html/RadioNodeList.h"

namespace WebCore {

template<typename CallbackInfo>
static v8::Handle<v8::Value> getNamedItems(HTMLFormControlsCollection* collection, const AtomicString& name, const CallbackInfo& callbackInfo)
{
    Vector<RefPtr<Node> > namedItems;
    collection->namedItems(name, namedItems);

    if (!namedItems.size())
        return v8Undefined();

    if (namedItems.size() == 1)
        return toV8(namedItems.at(0).release(), callbackInfo.Holder(), callbackInfo.GetIsolate());

    return toV8(collection->ownerNode()->radioNodeList(name).get(), callbackInfo.Holder(), callbackInfo.GetIsolate());
}

void V8HTMLFormControlsCollection::namedItemMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, name, args[0]);
    HTMLFormControlsCollection* imp = V8HTMLFormControlsCollection::toNative(args.Holder());
    v8::Handle<v8::Value> result = getNamedItems(imp, name, args);

    if (result.IsEmpty()) {
        v8SetReturnValueNull(args);
        return;
    }

    v8SetReturnValue(args, result);
}

} // namespace WebCore
