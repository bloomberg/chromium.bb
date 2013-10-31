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
#include "V8SQLTransaction.h"

#include "V8SQLStatementCallback.h"
#include "V8SQLStatementErrorCallback.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webdatabase/sqlite/SQLValue.h"
#include "modules/webdatabase/Database.h"
#include "wtf/Vector.h"

using namespace WTF;

namespace WebCore {

void V8SQLTransaction::executeSqlMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (!info.Length()) {
        setDOMException(SyntaxError, info.GetIsolate());
        return;
    }

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, statement, info[0]);

    Vector<SQLValue> sqlValues;

    if (info.Length() > 1 && !isUndefinedOrNull(info[1])) {
        if (!info[1]->IsObject()) {
            setDOMException(TypeMismatchError, info.GetIsolate());
            return;
        }

        uint32_t sqlArgsLength = 0;
        v8::Local<v8::Object> sqlArgsObject = info[1]->ToObject();
        V8TRYCATCH_VOID(v8::Local<v8::Value>, length, sqlArgsObject->Get(v8::String::NewSymbol("length")));

        if (isUndefinedOrNull(length))
            sqlArgsLength = sqlArgsObject->GetPropertyNames()->Length();
        else
            sqlArgsLength = length->Uint32Value();

        for (unsigned int i = 0; i < sqlArgsLength; ++i) {
            v8::Handle<v8::Integer> key = v8::Integer::New(i, info.GetIsolate());
            V8TRYCATCH_VOID(v8::Local<v8::Value>, value, sqlArgsObject->Get(key));

            if (value.IsEmpty() || value->IsNull())
                sqlValues.append(SQLValue());
            else if (value->IsNumber()) {
                V8TRYCATCH_VOID(double, sqlValue, value->NumberValue());
                sqlValues.append(SQLValue(sqlValue));
            } else {
                V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, sqlValue, value);
                sqlValues.append(SQLValue(sqlValue));
            }
        }
    }

    SQLTransaction* transaction = V8SQLTransaction::toNative(info.Holder());

    ExecutionContext* executionContext = getExecutionContext();

    RefPtr<SQLStatementCallback> callback;
    if (info.Length() > 2 && !isUndefinedOrNull(info[2])) {
        if (!info[2]->IsObject()) {
            setDOMException(TypeMismatchError, info.GetIsolate());
            return;
        }
        callback = V8SQLStatementCallback::create(info[2], executionContext);
    }

    RefPtr<SQLStatementErrorCallback> errorCallback;
    if (info.Length() > 3 && !isUndefinedOrNull(info[3])) {
        if (!info[3]->IsObject()) {
            setDOMException(TypeMismatchError, info.GetIsolate());
            return;
        }
        errorCallback = V8SQLStatementErrorCallback::create(info[3], executionContext);
    }

    ExceptionState es(info.GetIsolate());
    transaction->executeSQL(statement, sqlValues, callback, errorCallback, es);
    es.throwIfNeeded();
}

} // namespace WebCore
