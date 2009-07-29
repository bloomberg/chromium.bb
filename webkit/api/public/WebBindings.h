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

#ifndef WebBindings_h
#define WebBindings_h

#include <bindings/npruntime.h>
#include "WebCommon.h"

namespace WebKit {

    class WebDragData;

    // A haphazard collection of functions for dealing with plugins.
    class WebBindings {
    public:
        // NPN Functions ------------------------------------------------------
        // These are all defined in npruntime.h and are well documented.

        // NPN_CreateObject
        WEBKIT_API static NPObject* createObject(NPP npp, NPClass* npClass);

        // NPN_GetIntIdentifier
        WEBKIT_API static NPIdentifier getIntIdentifier(int32_t);

        // NPN_GetProperty
        WEBKIT_API static bool getProperty(NPP npp, NPObject* obj, NPIdentifier propertyName, NPVariant *result);

        // NPN_GetStringIdentifier
        WEBKIT_API static NPIdentifier getStringIdentifier(const NPUTF8*);

        // NPN_HasMethod
        WEBKIT_API static bool hasMethod(NPP npp, NPObject* npObject, NPIdentifier methodName);

        // NPN_HasProperty
        WEBKIT_API static bool hasProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName);

        // NPN_InitializeVariantWithStringCopy (though sometimes prefixed with an underscore)
        WEBKIT_API static void initializeVariantWithStringCopy(NPVariant* variant, const NPString* value);

        // NPN_Invoke
        WEBKIT_API static bool invoke(NPP npp, NPObject* npObject, NPIdentifier methodName, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result);

        // NPN_ReleaseObject
        WEBKIT_API static void releaseObject(NPObject* npObject);

        // NPN_ReleaseVariantValue
        WEBKIT_API static void releaseVariantValue(NPVariant* variant);

        // NPN_RetainObject
        WEBKIT_API static NPObject* retainObject(NPObject* npObject);

        // _NPN_UnregisterObject
        WEBKIT_API static void unregisterObject(NPObject* npObject);

        // Miscellaneous utility functions ------------------------------------

        // Complement to NPN_Get___Identifier functions.  Extracts data from the NPIdentifier data
        // structure.  If isString is true upon return, string will be set but number's value is
        // undefined.  If iString is false, the opposite is true.
        WEBKIT_API static void extractIdentifierData(const NPIdentifier&, const NPUTF8*& string, int32_t& number, bool& isString);

        // Return true (success) if the given npobj is the current drag event in browser dispatch,
        // and is accessible based on context execution frames and their security origins and
        // WebKit clipboard access policy. If so, return the event id and the clipboard data (WebDragData).
        // This only works with V8.  If compiled without V8, it'll always return false.
        WEBKIT_API static bool getDragData(NPObject* event, int* event_id, WebDragData* data);

        // Invoke the event access policy checks listed above with GetDragData().  No need for clipboard
        // data or event_id outputs, just confirm the given npobj is the current & accessible drag event.
        // This only works with V8.  If compiled without V8, it'll always return false.
        WEBKIT_API static bool isDragEvent(NPObject* event);
    };

} // namespace WebKit

#endif
