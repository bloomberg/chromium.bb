/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/fileapi/Blob.h"

#include "V8Blob.h"
#include "V8File.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8Utilities.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "core/fileapi/BlobBuilder.h"
#include "wtf/RefPtr.h"

namespace WebCore {

v8::Handle<v8::Object> wrap(Blob* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    if (impl->isFile())
        return wrap(toFile(impl), creationContext, isolate);
    return V8Blob::createWrapper(impl, creationContext, isolate);
}

void V8Blob::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (!args.Length()) {
        RefPtr<Blob> blob = Blob::create();
        args.GetReturnValue().Set(toV8(blob.get(), args.Holder(), args.GetIsolate()));
        return;
    }

    v8::Local<v8::Value> firstArg = args[0];
    if (!firstArg->IsArray()) {
        throwTypeError("First argument of the constructor is not of type Array", args.GetIsolate());
        return;
    }

    String type;
    String endings = "transparent";

    if (args.Length() > 1) {
        if (!args[1]->IsObject()) {
            throwTypeError("Second argument of the constructor is not of type Object", args.GetIsolate());
            return;
        }

        V8TRYCATCH_VOID(Dictionary, dictionary, Dictionary(args[1], args.GetIsolate()));

        V8TRYCATCH_VOID(bool, containsEndings, dictionary.get("endings", endings));
        if (containsEndings) {
            if (endings != "transparent" && endings != "native") {
                throwTypeError("The endings property must be either \"transparent\" or \"native\"", args.GetIsolate());
                return;
            }
        }

        V8TRYCATCH_VOID(bool, containsType, dictionary.get("type", type));
        UNUSED_PARAM(containsType);
        if (!type.containsOnlyASCII()) {
            throwError(v8SyntaxError, "type must consist of ASCII characters", args.GetIsolate());
            return;
        }
        type.makeLower();
    }

    ASSERT(endings == "transparent" || endings == "native");

    BlobBuilder blobBuilder;

    V8TRYCATCH_VOID(v8::Local<v8::Array>, blobParts, v8::Local<v8::Array>::Cast(firstArg));
    uint32_t length = blobParts->Length();

    for (uint32_t i = 0; i < length; ++i) {
        v8::Local<v8::Value> item = blobParts->Get(v8::Uint32::New(i));
        ASSERT(!item.IsEmpty());
        if (V8ArrayBuffer::HasInstance(item, args.GetIsolate(), worldType(args.GetIsolate()))) {
            ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(v8::Handle<v8::Object>::Cast(item));
            ASSERT(arrayBuffer);
            blobBuilder.append(arrayBuffer);
        } else if (V8ArrayBufferView::HasInstance(item, args.GetIsolate(), worldType(args.GetIsolate()))) {
            ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(v8::Handle<v8::Object>::Cast(item));
            ASSERT(arrayBufferView);
            blobBuilder.append(arrayBufferView);
        } else if (V8Blob::HasInstance(item, args.GetIsolate(), worldType(args.GetIsolate()))) {
            Blob* blob = V8Blob::toNative(v8::Handle<v8::Object>::Cast(item));
            ASSERT(blob);
            blobBuilder.append(blob);
        } else {
            V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, stringValue, item);
            blobBuilder.append(stringValue, endings);
        }
    }

    RefPtr<Blob> blob = blobBuilder.getBlob(type);
    args.GetReturnValue().Set(toV8(blob.get(), args.Holder(), args.GetIsolate()));
}

} // namespace WebCore
