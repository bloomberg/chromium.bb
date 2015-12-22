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

#include "public/web/WebBindings.h"

#include "bindings/core/v8/NPV8Object.h"
#include "bindings/core/v8/npruntime_impl.h"
#include "bindings/core/v8/npruntime_priv.h"

namespace blink {

bool WebBindings::construct(NPP npp, NPObject* object, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return _NPN_Construct(npp, object, args, argCount, result);
}

NPObject* WebBindings::createObject(NPP npp, NPClass* npClass)
{
    return _NPN_CreateObject(npp, npClass);
}

bool WebBindings::enumerate(NPP npp, NPObject* object, NPIdentifier** identifier, uint32_t* identifierCount)
{
    return _NPN_Enumerate(npp, object, identifier, identifierCount);
}

bool WebBindings::evaluate(NPP npp, NPObject* object, NPString* script, NPVariant* result)
{
    return _NPN_Evaluate(npp, object, script, result);
}

bool WebBindings::evaluateHelper(NPP npp, bool popupsAllowed, NPObject* object, NPString* script, NPVariant* result)
{
    return _NPN_EvaluateHelper(npp, popupsAllowed, object, script, result);
}

NPIdentifier WebBindings::getIntIdentifier(int32_t number)
{
    return _NPN_GetIntIdentifier(number);
}

bool WebBindings::getProperty(NPP npp, NPObject* object, NPIdentifier property, NPVariant* result)
{
    return _NPN_GetProperty(npp, object, property, result);
}

NPIdentifier WebBindings::getStringIdentifier(const NPUTF8* string)
{
    return _NPN_GetStringIdentifier(string);
}

void WebBindings::getStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers)
{
    _NPN_GetStringIdentifiers(names, nameCount, identifiers);
}

bool WebBindings::hasMethod(NPP npp, NPObject* object, NPIdentifier method)
{
    return _NPN_HasMethod(npp, object, method);
}

bool WebBindings::hasProperty(NPP npp, NPObject* object, NPIdentifier property)
{
    return _NPN_HasProperty(npp, object, property);
}

bool WebBindings::identifierIsString(NPIdentifier identifier)
{
    return _NPN_IdentifierIsString(identifier);
}

int32_t WebBindings::intFromIdentifier(NPIdentifier identifier)
{
    return _NPN_IntFromIdentifier(identifier);
}

bool WebBindings::invoke(NPP npp, NPObject* object, NPIdentifier method, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return _NPN_Invoke(npp, object, method, args, argCount, result);
}

bool WebBindings::invokeDefault(NPP npp, NPObject* object, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return _NPN_InvokeDefault(npp, object, args, argCount, result);
}

void WebBindings::releaseObject(NPObject* object)
{
    return _NPN_ReleaseObject(object);
}

void WebBindings::releaseVariantValue(NPVariant* variant)
{
    _NPN_ReleaseVariantValue(variant);
}

bool WebBindings::removeProperty(NPP npp, NPObject* object, NPIdentifier identifier)
{
    return _NPN_RemoveProperty(npp, object, identifier);
}

NPObject* WebBindings::retainObject(NPObject* object)
{
    return _NPN_RetainObject(object);
}

void WebBindings::setException(NPObject* object, const NPUTF8* message)
{
    _NPN_SetException(object, message);
}

bool WebBindings::setProperty(NPP npp, NPObject* object, NPIdentifier identifier, const NPVariant* value)
{
    return _NPN_SetProperty(npp, object, identifier, value);
}

NPUTF8* WebBindings::utf8FromIdentifier(NPIdentifier identifier)
{
    return _NPN_UTF8FromIdentifier(identifier);
}

void WebBindings::extractIdentifierData(const NPIdentifier& identifier, const NPUTF8*& string, int32_t& number, bool& isString)
{
    PrivateIdentifier* data = static_cast<PrivateIdentifier*>(identifier);
    if (!data) {
        isString = false;
        number = 0;
        return;
    }

    isString = data->isString;
    if (isString)
        string = data->value.string;
    else
        number = data->value.number;
}

} // namespace blink
