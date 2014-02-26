/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/V8Utilities.h"

#include "V8MessagePort.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/text/WTFString.h"
#include <v8.h>


namespace WebCore {

bool extractTransferables(v8::Local<v8::Value> value, int argumentIndex, MessagePortArray& ports, ArrayBufferArray& arrayBuffers, ExceptionState& exceptionState, v8::Isolate* isolate)
{
    if (isUndefinedOrNull(value)) {
        ports.resize(0);
        arrayBuffers.resize(0);
        return true;
    }

    uint32_t length = 0;
    if (value->IsArray()) {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
        length = array->Length();
    } else if (toV8Sequence(value, length, isolate).IsEmpty()) {
        exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex + 1));
        return false;
    }

    v8::Local<v8::Object> transferrables = v8::Local<v8::Object>::Cast(value);

    // Validate the passed array of transferrables.
    for (unsigned int i = 0; i < length; ++i) {
        v8::Local<v8::Value> transferrable = transferrables->Get(i);
        // Validation of non-null objects, per HTML5 spec 10.3.3.
        if (isUndefinedOrNull(transferrable)) {
            exceptionState.throwDOMException(DataCloneError, "Value at index " + String::number(i) + " is an untransferable " + (transferrable->IsUndefined() ? "'undefined'" : "'null'") + " value.");
            return false;
        }
        // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
        if (V8MessagePort::hasInstance(transferrable, isolate)) {
            RefPtr<MessagePort> port = V8MessagePort::toNative(v8::Handle<v8::Object>::Cast(transferrable));
            // Check for duplicate MessagePorts.
            if (ports.contains(port)) {
                exceptionState.throwDOMException(DataCloneError, "Message port at index " + String::number(i) + " is a duplicate of an earlier port.");
                return false;
            }
            ports.append(port.release());
        } else if (V8ArrayBuffer::hasInstance(transferrable, isolate)) {
            RefPtr<ArrayBuffer> arrayBuffer = V8ArrayBuffer::toNative(v8::Handle<v8::Object>::Cast(transferrable));
            if (arrayBuffers.contains(arrayBuffer)) {
                exceptionState.throwDOMException(DataCloneError, "ArrayBuffer at index " + String::number(i) + " is a duplicate of an earlier ArrayBuffer.");
                return false;
            }
            arrayBuffers.append(arrayBuffer.release());
        } else {
            exceptionState.throwDOMException(DataCloneError, "Value at index " + String::number(i) + " does not have a transferable type.");
            return false;
        }
    }
    return true;
}

} // namespace WebCore
