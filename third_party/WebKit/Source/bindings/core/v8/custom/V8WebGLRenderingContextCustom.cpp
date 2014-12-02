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
#include "bindings/core/v8/V8WebGLRenderingContext.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8WebGLProgram.h"
#include "bindings/core/v8/V8WebGLShader.h"
#include "bindings/core/v8/V8WebGLUniformLocation.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/canvas/WebGLExtension.h"
#include "core/html/canvas/WebGLRenderingContext.h"
#include "platform/NotImplemented.h"

namespace blink {

static v8::Handle<v8::Value> toV8Object(const WebGLGetInfo& args, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (args.getType()) {
    case WebGLGetInfo::kTypeBool:
        return v8Boolean(args.getBool(), isolate);
    case WebGLGetInfo::kTypeBoolArray: {
        const Vector<bool>& value = args.getBoolArray();
        v8::Local<v8::Array> array = v8::Array::New(isolate, value.size());
        for (size_t ii = 0; ii < value.size(); ++ii)
            array->Set(v8::Integer::New(isolate, ii), v8Boolean(value[ii], isolate));
        return array;
    }
    case WebGLGetInfo::kTypeFloat:
        return v8::Number::New(isolate, args.getFloat());
    case WebGLGetInfo::kTypeInt:
        return v8::Integer::New(isolate, args.getInt());
    case WebGLGetInfo::kTypeNull:
        return v8::Null(isolate);
    case WebGLGetInfo::kTypeString:
        return v8String(isolate, args.getString());
    case WebGLGetInfo::kTypeUnsignedInt:
        return v8::Integer::NewFromUnsigned(isolate, args.getUnsignedInt());
    case WebGLGetInfo::kTypeWebGLObject:
        return toV8(args.getWebGLObject(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLFloatArray:
        return toV8(args.getWebGLFloatArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLIntArray:
        return toV8(args.getWebGLIntArray(), creationContext, isolate);
    // FIXME: implement WebGLObjectArray
    // case WebGLGetInfo::kTypeWebGLObjectArray:
    case WebGLGetInfo::kTypeWebGLUnsignedByteArray:
        return toV8(args.getWebGLUnsignedByteArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLUnsignedIntArray:
        return toV8(args.getWebGLUnsignedIntArray(), creationContext, isolate);
    default:
        notImplemented();
        return v8::Undefined(isolate);
    }
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static void getObjectParameter(const v8::FunctionCallbackInfo<v8::Value>& info, ObjectType objectType, ExceptionState& exceptionState)
{
    if (info.Length() != 2) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(2, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toImpl(info.Holder());
    unsigned target;
    unsigned pname;
    {
        v8::TryCatch block;
        V8RethrowTryCatchScope rethrow(block);
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(target, toUInt32(info[0], exceptionState), exceptionState);
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(pname, toUInt32(info[1], exceptionState), exceptionState);
    }
    WebGLGetInfo args;
    switch (objectType) {
    case kBuffer:
        args = context->getBufferParameter(target, pname);
        break;
    case kRenderbuffer:
        args = context->getRenderbufferParameter(target, pname);
        break;
    case kTexture:
        args = context->getTexParameter(target, pname);
        break;
    case kVertexAttrib:
        // target => index
        args = context->getVertexAttrib(target, pname);
        break;
    default:
        notImplemented();
        break;
    }
    v8SetReturnValue(info, toV8Object(args, info.Holder(), info.GetIsolate()));
}

void V8WebGLRenderingContext::getBufferParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getBufferParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    getObjectParameter(info, kBuffer, exceptionState);
}

void V8WebGLRenderingContext::getExtensionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getExtension", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    WebGLRenderingContext* impl = V8WebGLRenderingContext::toImpl(info.Holder());
    if (info.Length() < 1) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(1, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }
    TOSTRING_VOID(V8StringResource<>, name, info[0]);
    RefPtrWillBeRawPtr<WebGLExtension> extension(impl->getExtension(name));
    v8::Handle<v8::Value> extensionObject = toV8(extension.get(), info.Holder(), info.GetIsolate());
    if (extension) {
        V8HiddenValue::setHiddenValue(info.GetIsolate(), info.Holder(), v8AtomicString(info.GetIsolate(), static_cast<String>(name).utf8().data()), extensionObject);
    }
    v8SetReturnValue(info, extensionObject);
}

void V8WebGLRenderingContext::getFramebufferAttachmentParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getFramebufferAttachmentParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    if (info.Length() != 3) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(3, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toImpl(info.Holder());
    unsigned target;
    unsigned attachment;
    unsigned pname;
    {
        v8::TryCatch block;
        V8RethrowTryCatchScope rethrow(block);
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(target, toUInt32(info[0], exceptionState), exceptionState);
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(attachment, toUInt32(info[1], exceptionState), exceptionState);
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(pname, toUInt32(info[2], exceptionState), exceptionState);
    }
    WebGLGetInfo args = context->getFramebufferAttachmentParameter(target, attachment, pname);
    v8SetReturnValue(info, toV8Object(args, info.Holder(), info.GetIsolate()));
}

void V8WebGLRenderingContext::getParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    if (info.Length() != 1) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(1, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toImpl(info.Holder());
    unsigned pname;
    {
        v8::TryCatch block;
        V8RethrowTryCatchScope rethrow(block);
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(pname, toUInt32(info[0], exceptionState), exceptionState);
    }
    WebGLGetInfo args = context->getParameter(pname);
    v8SetReturnValue(info, toV8Object(args, info.Holder(), info.GetIsolate()));
}

void V8WebGLRenderingContext::getProgramParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getProgramParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    if (info.Length() != 2) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(2, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toImpl(info.Holder());
    WebGLProgram* program;
    unsigned pname;
    {
        v8::TryCatch block;
        V8RethrowTryCatchScope rethrow(block);
        if (info.Length() > 0 && !isUndefinedOrNull(info[0]) && !V8WebGLProgram::hasInstance(info[0], info.GetIsolate())) {
            exceptionState.throwTypeError("parameter 1 is not of type 'WebGLProgram'.");
            exceptionState.throwIfNeeded();
            return;
        }
        TONATIVE_VOID_INTERNAL(program, V8WebGLProgram::toImplWithTypeCheck(info.GetIsolate(), info[0]));
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(pname, toUInt32(info[1], exceptionState), exceptionState);
    }
    WebGLGetInfo args = context->getProgramParameter(program, pname);
    v8SetReturnValue(info, toV8Object(args, info.Holder(), info.GetIsolate()));
}

void V8WebGLRenderingContext::getRenderbufferParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getRenderbufferParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    getObjectParameter(info, kRenderbuffer, exceptionState);
}

void V8WebGLRenderingContext::getShaderParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getShaderParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    if (info.Length() != 2) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(2, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toImpl(info.Holder());
    WebGLShader* shader;
    unsigned pname;
    {
        v8::TryCatch block;
        V8RethrowTryCatchScope rethrow(block);
        if (info.Length() > 0 && !isUndefinedOrNull(info[0]) && !V8WebGLShader::hasInstance(info[0], info.GetIsolate())) {
            exceptionState.throwTypeError("parameter 1 is not of type 'WebGLShader'.");
            exceptionState.throwIfNeeded();
            return;
        }
        TONATIVE_VOID_INTERNAL(shader, V8WebGLShader::toImplWithTypeCheck(info.GetIsolate(), info[0]));
        TONATIVE_VOID_EXCEPTIONSTATE_INTERNAL(pname, toUInt32(info[1], exceptionState), exceptionState);
    }
    WebGLGetInfo args = context->getShaderParameter(shader, pname);
    v8SetReturnValue(info, toV8Object(args, info.Holder(), info.GetIsolate()));
}

void V8WebGLRenderingContext::getTexParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getTexParameter", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    getObjectParameter(info, kTexture, exceptionState);
}

void V8WebGLRenderingContext::getUniformMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getUniform", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    if (info.Length() != 2) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(2, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toImpl(info.Holder());
    WebGLProgram* program;
    WebGLUniformLocation* location;
    {
        v8::TryCatch block;
        V8RethrowTryCatchScope rethrow(block);
        if (info.Length() > 0 && !isUndefinedOrNull(info[0]) && !V8WebGLProgram::hasInstance(info[0], info.GetIsolate())) {
            V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToExecute("getUniform", "WebGLRenderingContext", "parameter 1 is not of type 'WebGLProgram'."));
            return;
        }
        TONATIVE_VOID_INTERNAL(program, V8WebGLProgram::toImplWithTypeCheck(info.GetIsolate(), info[0]));
        if (info.Length() > 1 && !isUndefinedOrNull(info[1]) && !V8WebGLUniformLocation::hasInstance(info[1], info.GetIsolate())) {
            V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToExecute("getUniform", "WebGLRenderingContext", "parameter 2 is not of type 'WebGLUniformLocation'."));
            return;
        }
        TONATIVE_VOID_INTERNAL(location, V8WebGLUniformLocation::toImplWithTypeCheck(info.GetIsolate(), info[1]));
    }
    WebGLGetInfo args = context->getUniform(program, location);
    v8SetReturnValue(info, toV8Object(args, info.Holder(), info.GetIsolate()));
}

void V8WebGLRenderingContext::getVertexAttribMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getVertexAttrib", "WebGLRenderingContext", info.Holder(), info.GetIsolate());
    getObjectParameter(info, kVertexAttrib, exceptionState);
}

} // namespace blink
