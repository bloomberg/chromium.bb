/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "V8File.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/custom/V8BlobCustomHelpers.h"
#include "core/fileapi/BlobBuilder.h"

namespace WebCore {

void V8File::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (!RuntimeEnabledFeatures::fileConstructorEnabled()) {
        throwTypeError("Illegal constructor", info.GetIsolate());
        return;
    }

    if (info.Length() < 2) {
        throwTypeError("File constructor requires at least two arguments", info.GetIsolate());
        return;
    }

    uint32_t length = 0;
    if (info[0]->IsArray()) {
        length = v8::Local<v8::Array>::Cast(info[0])->Length();
    } else {
        const int sequenceArgumentIndex = 0;
        if (toV8Sequence(info[sequenceArgumentIndex], length, info.GetIsolate()).IsEmpty()) {
            throwTypeError(ExceptionMessages::failedToConstruct("File", ExceptionMessages::notAnArrayTypeArgumentOrValue(sequenceArgumentIndex + 1)), info.GetIsolate());
            return;
        }
    }

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, fileName, info[1]);

    String contentType;
    String endings = "transparent"; // default if no BlobPropertyBag is passed
    if (info.Length() > 2) {
        if (!info[2]->IsObject()) {
            throwTypeError(ExceptionMessages::failedToConstruct("File", "The 3rd argument is not of type Object."), info.GetIsolate());
            return;
        }

        if (!V8BlobCustomHelpers::processBlobPropertyBag(info[2], "File", contentType, endings, info.GetIsolate()))
            return;
    }

    BlobBuilder blobBuilder;
    v8::Local<v8::Object> blobParts = v8::Local<v8::Object>::Cast(info[0]);
    if (!V8BlobCustomHelpers::processBlobParts(blobParts, length, endings, blobBuilder, info.GetIsolate()))
        return;

    RefPtr<File> file = blobBuilder.createFile(contentType, fileName, currentTime());
    v8SetReturnValue(info, file.release());
}

} // namespace WebCore
