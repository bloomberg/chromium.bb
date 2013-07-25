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
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/page/Frame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/text/WTFString.h"
#include <v8.h>


namespace WebCore {

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the DOM object.
void createHiddenDependency(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int cacheIndex, v8::Isolate* isolate)
{
    v8::Local<v8::Value> cache = object->GetInternalField(cacheIndex);
    if (cache->IsNull() || cache->IsUndefined()) {
        cache = v8::Array::New();
        object->SetInternalField(cacheIndex, cache);
    }

    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    cacheArray->Set(v8::Integer::New(cacheArray->Length(), isolate), value);
}

bool extractTransferables(v8::Local<v8::Value> value, MessagePortArray& ports, ArrayBufferArray& arrayBuffers, v8::Isolate* isolate)
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
    } else {
        if (toV8Sequence(value, length, isolate).IsEmpty())
            return false;
    }

    v8::Local<v8::Object> transferrables = v8::Local<v8::Object>::Cast(value);

    // Validate the passed array of transferrables.
    for (unsigned int i = 0; i < length; ++i) {
        v8::Local<v8::Value> transferrable = transferrables->Get(i);
        // Validation of non-null objects, per HTML5 spec 10.3.3.
        if (isUndefinedOrNull(transferrable)) {
            setDOMException(DataCloneError, isolate);
            return false;
        }
        // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
        if (V8MessagePort::HasInstance(transferrable, isolate, worldType(isolate))) {
            RefPtr<MessagePort> port = V8MessagePort::toNative(v8::Handle<v8::Object>::Cast(transferrable));
            // Check for duplicate MessagePorts.
            if (ports.contains(port)) {
                setDOMException(DataCloneError, isolate);
                return false;
            }
            ports.append(port.release());
        } else if (V8ArrayBuffer::HasInstance(transferrable, isolate, worldType(isolate)))
            arrayBuffers.append(V8ArrayBuffer::toNative(v8::Handle<v8::Object>::Cast(transferrable)));
        else {
            setDOMException(DataCloneError, isolate);
            return false;
        }
    }
    return true;
}

bool getMessagePortArray(v8::Local<v8::Value> value, MessagePortArray& ports, v8::Isolate* isolate)
{
    if (isUndefinedOrNull(value)) {
        ports.resize(0);
        return true;
    }
    if (!value->IsArray()) {
        throwTypeError(isolate);
        return false;
    }
    bool success = false;
    ports = toRefPtrNativeArray<MessagePort, V8MessagePort>(value, isolate, &success);
    return success;
}

void removeHiddenDependency(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int cacheIndex, v8::Isolate* isolate)
{
    v8::Local<v8::Value> cache = object->GetInternalField(cacheIndex);
    if (!cache->IsArray())
        return;
    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    for (int i = cacheArray->Length() - 1; i >= 0; --i) {
        v8::Local<v8::Value> cached = cacheArray->Get(v8::Integer::New(i, isolate));
        if (cached->StrictEquals(value)) {
            cacheArray->Delete(i);
            return;
        }
    }
}

void transferHiddenDependency(v8::Handle<v8::Object> object, EventListener* oldValue, v8::Local<v8::Value> newValue, int cacheIndex, v8::Isolate* isolate)
{
    if (oldValue) {
        V8AbstractEventListener* oldListener = V8AbstractEventListener::cast(oldValue);
        if (oldListener) {
            v8::Local<v8::Object> oldListenerObject = oldListener->getExistingListenerObject();
            if (!oldListenerObject.IsEmpty())
                removeHiddenDependency(object, oldListenerObject, cacheIndex, isolate);
        }
    }
    if (!newValue->IsNull() && !newValue->IsUndefined())
        createHiddenDependency(object, newValue, cacheIndex, isolate);
}

ScriptExecutionContext* getScriptExecutionContext()
{
    if (WorkerScriptController* controller = WorkerScriptController::controllerForContext())
        return controller->workerGlobalScope();

    return currentDocument();
}

} // namespace WebCore
