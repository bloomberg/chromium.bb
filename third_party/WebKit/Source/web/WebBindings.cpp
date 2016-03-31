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

namespace blink {

bool WebBindings::construct(NPP npp, NPObject* object, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return false;
}

NPObject* WebBindings::createObject(NPP npp, NPClass* npClass)
{
    return nullptr;
}

bool WebBindings::enumerate(NPP npp, NPObject* object, NPIdentifier** identifier, uint32_t* identifierCount)
{
    return false;
}

bool WebBindings::evaluate(NPP npp, NPObject* object, NPString* script, NPVariant* result)
{
    return false;
}

NPIdentifier WebBindings::getIntIdentifier(int32_t number)
{
    return nullptr;
}

bool WebBindings::getProperty(NPP npp, NPObject* object, NPIdentifier property, NPVariant* result)
{
    return false;
}

NPIdentifier WebBindings::getStringIdentifier(const NPUTF8* string)
{
    return nullptr;
}

void WebBindings::getStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers)
{
}

bool WebBindings::hasMethod(NPP npp, NPObject* object, NPIdentifier method)
{
    return false;
}

bool WebBindings::hasProperty(NPP npp, NPObject* object, NPIdentifier property)
{
    return false;
}

bool WebBindings::identifierIsString(NPIdentifier identifier)
{
    return false;
}

int32_t WebBindings::intFromIdentifier(NPIdentifier identifier)
{
    return 0;
}

bool WebBindings::invoke(NPP npp, NPObject* object, NPIdentifier method, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return false;
}

bool WebBindings::invokeDefault(NPP npp, NPObject* object, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return false;
}

void WebBindings::releaseObject(NPObject* object)
{
}

void WebBindings::releaseVariantValue(NPVariant* variant)
{
}

bool WebBindings::removeProperty(NPP npp, NPObject* object, NPIdentifier identifier)
{
    return false;
}

NPObject* WebBindings::retainObject(NPObject* object)
{
    return nullptr;
}

void WebBindings::setException(NPObject* object, const NPUTF8* message)
{
}

bool WebBindings::setProperty(NPP npp, NPObject* object, NPIdentifier identifier, const NPVariant* value)
{
    return false;
}

NPUTF8* WebBindings::utf8FromIdentifier(NPIdentifier identifier)
{
    return nullptr;
}

} // namespace blink
