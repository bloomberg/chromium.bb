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
#include "V8SVGLength.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/properties/SVGPropertyTearOff.h"

namespace WebCore {

void V8SVGLength::valueAttrGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    SVGLength& imp = wrapper->propertyReference();
    ExceptionState es(info.GetIsolate());
    SVGLengthContext lengthContext(wrapper->contextElement());
    float value = imp.value(lengthContext, es);
    if (es.throwIfNeeded())
        return;
    v8SetReturnValue(info, value);
}

void V8SVGLength::valueAttrSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    if (wrapper->isReadOnly()) {
        setDOMException(NoModificationAllowedError, info.GetIsolate());
        return;
    }

    if (!isUndefinedOrNull(value) && !value->IsNumber() && !value->IsBoolean()) {
        throwTypeError(info.GetIsolate());
        return;
    }

    SVGLength& imp = wrapper->propertyReference();
    ExceptionState es(info.GetIsolate());
    SVGLengthContext lengthContext(wrapper->contextElement());
    imp.setValue(static_cast<float>(value->NumberValue()), lengthContext, es);
    if (es.throwIfNeeded())
        return;
    wrapper->commitChange();
}

void V8SVGLength::convertToSpecifiedUnitsMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(args.Holder());
    if (wrapper->isReadOnly()) {
        setDOMException(NoModificationAllowedError, args.GetIsolate());
        return;
    }

    if (args.Length() < 1) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    SVGLength& imp = wrapper->propertyReference();
    ExceptionState es(args.GetIsolate());
    V8TRYCATCH_VOID(int, unitType, toUInt32(args[0]));
    SVGLengthContext lengthContext(wrapper->contextElement());
    imp.convertToSpecifiedUnits(unitType, lengthContext, es);
    if (es.throwIfNeeded())
        return;
    wrapper->commitChange();
}

} // namespace WebCore
