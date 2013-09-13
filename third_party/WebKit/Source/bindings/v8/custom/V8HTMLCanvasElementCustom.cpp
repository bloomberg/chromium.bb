/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "V8HTMLCanvasElement.h"

#include "V8CanvasRenderingContext2D.h"
#include "V8Node.h"
#include "V8WebGLRenderingContext.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/Canvas2DContextAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/WebGLContextAttributes.h"
#include "core/inspector/InspectorCanvasInstrumentation.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

void V8HTMLCanvasElement::getContextMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLCanvasElement* imp = V8HTMLCanvasElement::toNative(holder);
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, contextIdResource, args[0]);
    String contextId = contextIdResource;
    RefPtr<CanvasContextAttributes> attributes;
    if (contextId == "webgl" || contextId == "experimental-webgl" || contextId == "webkit-3d") {
        RefPtr<WebGLContextAttributes> webGLAttributes = WebGLContextAttributes::create();
        if (args.Length() > 1 && args[1]->IsObject()) {
            v8::Handle<v8::Object> jsAttributes = args[1]->ToObject();
            v8::Handle<v8::String> alpha = v8::String::NewSymbol("alpha");
            if (jsAttributes->Has(alpha))
                webGLAttributes->setAlpha(jsAttributes->Get(alpha)->BooleanValue());
            v8::Handle<v8::String> depth = v8::String::NewSymbol("depth");
            if (jsAttributes->Has(depth))
                webGLAttributes->setDepth(jsAttributes->Get(depth)->BooleanValue());
            v8::Handle<v8::String> stencil = v8::String::NewSymbol("stencil");
            if (jsAttributes->Has(stencil))
                webGLAttributes->setStencil(jsAttributes->Get(stencil)->BooleanValue());
            v8::Handle<v8::String> antialias = v8::String::NewSymbol("antialias");
            if (jsAttributes->Has(antialias))
                webGLAttributes->setAntialias(jsAttributes->Get(antialias)->BooleanValue());
            v8::Handle<v8::String> premultipliedAlpha = v8::String::NewSymbol("premultipliedAlpha");
            if (jsAttributes->Has(premultipliedAlpha))
                webGLAttributes->setPremultipliedAlpha(jsAttributes->Get(premultipliedAlpha)->BooleanValue());
            v8::Handle<v8::String> preserveDrawingBuffer = v8::String::NewSymbol("preserveDrawingBuffer");
            if (jsAttributes->Has(preserveDrawingBuffer))
                webGLAttributes->setPreserveDrawingBuffer(jsAttributes->Get(preserveDrawingBuffer)->BooleanValue());
        }
        attributes = webGLAttributes;
    } else {
        RefPtr<Canvas2DContextAttributes> canvas2DAttributes = Canvas2DContextAttributes::create();
        if (args.Length() > 1 && args[1]->IsObject()) {
            v8::Handle<v8::Object> jsAttributes = args[1]->ToObject();
            v8::Handle<v8::String> alpha = v8::String::NewSymbol("alpha");
            if (jsAttributes->Has(alpha))
                canvas2DAttributes->setAlpha(jsAttributes->Get(alpha)->BooleanValue());
        }
        attributes = canvas2DAttributes;
    }
    CanvasRenderingContext* result = imp->getContext(contextId, attributes.get());
    if (!result) {
        v8SetReturnValueNull(args);
        return;
    }
    if (result->is2d()) {
        v8::Handle<v8::Value> v8Result = toV8(static_cast<CanvasRenderingContext2D*>(result), args.Holder(), args.GetIsolate());
        if (InspectorInstrumentation::canvasAgentEnabled(&imp->document())) {
            ScriptState* scriptState = ScriptState::forContext(v8::Context::GetCurrent());
            ScriptObject context(scriptState, v8::Handle<v8::Object>::Cast(v8Result));
            ScriptObject wrapped = InspectorInstrumentation::wrapCanvas2DRenderingContextForInstrumentation(&imp->document(), context);
            if (!wrapped.hasNoValue()) {
                v8SetReturnValue(args, wrapped.v8Value());
                return;
            }
        }
        v8SetReturnValue(args, v8Result);
        return;
    }
    if (result->is3d()) {
        v8::Handle<v8::Value> v8Result = toV8(static_cast<WebGLRenderingContext*>(result), args.Holder(), args.GetIsolate());
        if (InspectorInstrumentation::canvasAgentEnabled(&imp->document())) {
            ScriptState* scriptState = ScriptState::forContext(v8::Context::GetCurrent());
            ScriptObject glContext(scriptState, v8::Handle<v8::Object>::Cast(v8Result));
            ScriptObject wrapped = InspectorInstrumentation::wrapWebGLRenderingContextForInstrumentation(&imp->document(), glContext);
            if (!wrapped.hasNoValue()) {
                v8SetReturnValue(args, wrapped.v8Value());
                return;
            }
        }
        v8SetReturnValue(args, v8Result);
        return;
    }
    ASSERT_NOT_REACHED();
    v8SetReturnValueNull(args);
}

void V8HTMLCanvasElement::toDataURLMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLCanvasElement* canvas = V8HTMLCanvasElement::toNative(holder);
    ExceptionState es(args.GetIsolate());

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, args[0]);
    double quality;
    double* qualityPtr = 0;
    if (args.Length() > 1 && args[1]->IsNumber()) {
        quality = args[1]->NumberValue();
        qualityPtr = &quality;
    }

    String result = canvas->toDataURL(type, qualityPtr, es);
    es.throwIfNeeded();
    v8SetReturnValueStringOrUndefined(args, result, args.GetIsolate());
}

} // namespace WebCore
