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
#include "V8JavaScriptCallFrame.h"

#include "bindings/v8/V8Binding.h"

namespace WebCore {

void V8JavaScriptCallFrame::evaluateMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    String expression = toWebCoreStringWithUndefinedOrNullCheck(args[0]);
    v8SetReturnValue(args, impl->evaluate(expression));
}

void V8JavaScriptCallFrame::restartMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    v8SetReturnValue(args, impl->restart());
}

void V8JavaScriptCallFrame::setVariableValueMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    int scopeIndex = args[0]->Int32Value();
    String variableName = toWebCoreStringWithUndefinedOrNullCheck(args[1]);
    v8::Handle<v8::Value> newValue = args[2];
    v8SetReturnValue(args, impl->setVariableValue(scopeIndex, variableName, newValue));
}

void V8JavaScriptCallFrame::scopeChainAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(info.Holder());
    v8SetReturnValue(info, impl->scopeChain());
}

void V8JavaScriptCallFrame::scopeTypeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    int scopeIndex = args[0]->Int32Value();
    v8SetReturnValue(args, impl->scopeType(scopeIndex));
}

void V8JavaScriptCallFrame::thisObjectAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(info.Holder());
    v8SetReturnValue(info, impl->thisObject());
}

void V8JavaScriptCallFrame::typeAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8SetReturnValue(info, v8::String::NewSymbol("function"));
}

} // namespace WebCore

