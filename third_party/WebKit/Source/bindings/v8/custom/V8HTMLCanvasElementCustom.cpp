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
    String contextId = toWebCoreString(args[0]);
    RefPtr<CanvasContextAttributes> attrs;
    if (contextId == "webgl" || contextId == "experimental-webgl" || contextId == "webkit-3d") {
        RefPtr<WebGLContextAttributes> webGLAttrs = WebGLContextAttributes::create();
        if (args.Length() > 1 && args[1]->IsObject()) {
            v8::Handle<v8::Object> jsAttrs = args[1]->ToObject();
            v8::Handle<v8::String> alpha = v8::String::NewSymbol("alpha");
            if (jsAttrs->Has(alpha))
                webGLAttrs->setAlpha(jsAttrs->Get(alpha)->BooleanValue());
            v8::Handle<v8::String> depth = v8::String::NewSymbol("depth");
            if (jsAttrs->Has(depth))
                webGLAttrs->setDepth(jsAttrs->Get(depth)->BooleanValue());
            v8::Handle<v8::String> stencil = v8::String::NewSymbol("stencil");
            if (jsAttrs->Has(stencil))
                webGLAttrs->setStencil(jsAttrs->Get(stencil)->BooleanValue());
            v8::Handle<v8::String> antialias = v8::String::NewSymbol("antialias");
            if (jsAttrs->Has(antialias))
                webGLAttrs->setAntialias(jsAttrs->Get(antialias)->BooleanValue());
            v8::Handle<v8::String> premultipliedAlpha = v8::String::NewSymbol("premultipliedAlpha");
            if (jsAttrs->Has(premultipliedAlpha))
                webGLAttrs->setPremultipliedAlpha(jsAttrs->Get(premultipliedAlpha)->BooleanValue());
            v8::Handle<v8::String> preserveDrawingBuffer = v8::String::NewSymbol("preserveDrawingBuffer");
            if (jsAttrs->Has(preserveDrawingBuffer))
                webGLAttrs->setPreserveDrawingBuffer(jsAttrs->Get(preserveDrawingBuffer)->BooleanValue());
        }
        attrs = webGLAttrs;
    } else {
        RefPtr<Canvas2DContextAttributes> canvas2DAttrs = Canvas2DContextAttributes::create();
        if (args.Length() > 1 && args[1]->IsObject()) {
            v8::Handle<v8::Object> jsAttrs = args[1]->ToObject();
            v8::Handle<v8::String> alpha = v8::String::NewSymbol("alpha");
            if (jsAttrs->Has(alpha))
                canvas2DAttrs->setAlpha(jsAttrs->Get(alpha)->BooleanValue());
        }
        attrs = canvas2DAttrs;
    }
    CanvasRenderingContext* result = imp->getContext(contextId, attrs.get());
    if (!result) {
        v8SetReturnValueNull(args);
        return;
    }
    if (result->is2d()) {
        v8::Handle<v8::Value> v8Result = toV8Fast(static_cast<CanvasRenderingContext2D*>(result), args, imp);
        if (InspectorInstrumentation::canvasAgentEnabled(imp->document())) {
            ScriptState* scriptState = ScriptState::forContext(v8::Context::GetCurrent());
            ScriptObject context(scriptState, v8::Handle<v8::Object>::Cast(v8Result));
            ScriptObject wrapped = InspectorInstrumentation::wrapCanvas2DRenderingContextForInstrumentation(imp->document(), context);
            if (!wrapped.hasNoValue()) {
                v8SetReturnValue(args, wrapped.v8Value());
                return;
            }
        }
        v8SetReturnValue(args, v8Result);
        return;
    }
    if (result->is3d()) {
        v8::Handle<v8::Value> v8Result = toV8Fast(static_cast<WebGLRenderingContext*>(result), args, imp);
        if (InspectorInstrumentation::canvasAgentEnabled(imp->document())) {
            ScriptState* scriptState = ScriptState::forContext(v8::Context::GetCurrent());
            ScriptObject glContext(scriptState, v8::Handle<v8::Object>::Cast(v8Result));
            ScriptObject wrapped = InspectorInstrumentation::wrapWebGLRenderingContextForInstrumentation(imp->document(), glContext);
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

    String type = toWebCoreString(args[0]);
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
