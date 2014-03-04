/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "V8FormData.h"

#include "V8Blob.h"
#include "V8HTMLFormElement.h"
#include "bindings/v8/V8Binding.h"
#include "core/html/DOMFormData.h"

namespace WebCore {

void V8FormData::appendMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2) {
        throwError(v8SyntaxError, "Not enough arguments", info.GetIsolate());
        return;
    }

    DOMFormData* domFormData = V8FormData::toNative(info.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithNullCheck>, name, info[0]);

    v8::Handle<v8::Value> arg = info[1];
    if (V8Blob::hasInstance(arg, info.GetIsolate())) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
        Blob* blob = V8Blob::toNative(object);
        ASSERT(blob);

        String filename;
        if (info.Length() >= 3) {
            V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithUndefinedOrNullCheck>, filenameResource, info[2]);
            filename = filenameResource;
        }

        domFormData->append(name, blob, filename);
    } else {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, argString, arg);
        domFormData->append(name, argString);
    }
}

} // namespace WebCore
