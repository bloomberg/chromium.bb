/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/canvas/DataView.h"

#include "V8DataView.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"

namespace WebCore {

void V8DataView::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (!args.Length()) {
        // see constructWebGLArray -- we don't seem to be able to distingish between
        // 'new DataView()' and the call used to construct the cached DataView object.
        RefPtr<DataView> dataView = DataView::create(0);
        v8::Handle<v8::Object> wrapper = args.Holder();
        V8DOMWrapper::associateObjectWithWrapper(dataView.release(), &info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
        args.GetReturnValue().Set(wrapper);
        return;
    }
    if (args[0]->IsNull() || !V8ArrayBuffer::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(0, args.GetIsolate());
        return;
    }
    constructWebGLArrayWithArrayBufferArgument<DataView, char>(args, &info, v8::kExternalByteArray, false);
}

// FIXME: Don't need this override.
v8::Handle<v8::Object> wrap(DataView* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    return V8DataView::createWrapper(impl, creationContext, isolate);
}

} // namespace WebCore
