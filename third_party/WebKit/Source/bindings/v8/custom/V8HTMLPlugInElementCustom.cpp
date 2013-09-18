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

#include "V8HTMLAppletElement.h"
#include "V8HTMLEmbedElement.h"
#include "V8HTMLObjectElement.h"
#include "bindings/v8/ScriptInstance.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8NPObject.h"

namespace WebCore {

// FIXME: Consider moving getter/setter helpers to V8NPObject and renaming this file to V8PluginElementFunctions
// to match JSC bindings naming convention.

template <class C>
static void npObjectNamedGetter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    HTMLPlugInElement* imp = C::toNative(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return;

    v8::Local<v8::Object> instance = scriptInstance->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    npObjectGetNamedProperty(instance, name, info);
}

template <class C>
static void npObjectNamedSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    HTMLPlugInElement* imp = C::toNative(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return;

    v8::Local<v8::Object> instance = scriptInstance->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    npObjectSetNamedProperty(instance, name, value, info);
}

void V8HTMLAppletElement::namedPropertyGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectNamedGetter<V8HTMLAppletElement>(name, info);
}

void V8HTMLEmbedElement::namedPropertyGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectNamedGetter<V8HTMLEmbedElement>(name, info);
}

void V8HTMLObjectElement::namedPropertyGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectNamedGetter<V8HTMLObjectElement>(name, info);
}

void V8HTMLAppletElement::namedPropertySetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectNamedSetter<V8HTMLAppletElement>(name, value, info);
}

void V8HTMLEmbedElement::namedPropertySetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectNamedSetter<V8HTMLEmbedElement>(name, value, info);
}

void V8HTMLObjectElement::namedPropertySetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    return npObjectNamedSetter<V8HTMLObjectElement>(name, value, info);
}

void V8HTMLAppletElement::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    npObjectInvokeDefaultHandler(args);
}

void V8HTMLEmbedElement::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    npObjectInvokeDefaultHandler(args);
}

void V8HTMLObjectElement::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    npObjectInvokeDefaultHandler(args);
}

template <class C>
void npObjectIndexedGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    HTMLPlugInElement* imp = C::toNative(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return;

    v8::Local<v8::Object> instance = scriptInstance->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    npObjectGetIndexedProperty(instance, index, info);
}

template <class C>
void npObjectIndexedSetter(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    HTMLPlugInElement* imp = C::toNative(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return;

    v8::Local<v8::Object> instance = scriptInstance->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    npObjectSetIndexedProperty(instance, index, value, info);
}

void V8HTMLAppletElement::indexedPropertyGetterCustom(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectIndexedGetter<V8HTMLAppletElement>(index, info);
}

void V8HTMLEmbedElement::indexedPropertyGetterCustom(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectIndexedGetter<V8HTMLEmbedElement>(index, info);
}

void V8HTMLObjectElement::indexedPropertyGetterCustom(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectIndexedGetter<V8HTMLObjectElement>(index, info);
}

void V8HTMLAppletElement::indexedPropertySetterCustom(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectIndexedSetter<V8HTMLAppletElement>(index, value, info);
}

void V8HTMLEmbedElement::indexedPropertySetterCustom(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectIndexedSetter<V8HTMLEmbedElement>(index, value, info);
}

void V8HTMLObjectElement::indexedPropertySetterCustom(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    npObjectIndexedSetter<V8HTMLObjectElement>(index, value, info);
}

} // namespace WebCore
