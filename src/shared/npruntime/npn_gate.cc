
/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
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


// NaCl-NPAPI Interface

#include <stdlib.h>

#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include "native_client/src/shared/npruntime/npnavigator.h"

void* NPN_MemAlloc(uint32_t size) {
  return malloc(size);
}

void NPN_MemFree(void* ptr) {
  free(ptr);
}

void NPN_Status(NPP instance, const char* message) {
  if (instance == NULL || message == NULL) {
    return;
  }
  nacl::NPNavigator* navigator = NaClNP_GetNavigator();
  if (!navigator) {
    return;
  }
  navigator->SetStatus(message);
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void* value) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPNavigator* navigator = NaClNP_GetNavigator();
  if (!navigator) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  switch (variable) {
    case NPNVWindowNPObject:
      return navigator->GetValue(nacl::RPC_GET_WINDOW_OBJECT, value);
      break;
    case NPNVPluginElementNPObject:
      return navigator->GetValue(nacl::RPC_GET_PLUGIN_ELEMENT_OBJECT, value);
      break;
    default:
      break;
  }
  return NPERR_INVALID_PARAM;
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name) {
  nacl::NPNavigator* navigator = NaClNP_GetNavigator();
  if (!navigator) {
    return NULL;
  }
  return const_cast<char*>(navigator->GetStringIdentifier(name));
}

void NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount,
                              NPIdentifier* identifiers) {
  nacl::NPNavigator* navigator = NaClNP_GetNavigator();
  if (!navigator) {
    return;
  }
  for (int32_t i = 0; i < nameCount; ++i) {
    if (names[i]) {
      identifiers[i] =
        const_cast<char*>(navigator->GetStringIdentifier(names[i]));
    } else {
      identifiers[i] = NULL;
    }
  }
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid) {
  return reinterpret_cast<NPIdentifier>((intid << 1) | 1);
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
  return (reinterpret_cast<uintptr_t>(identifier) & 1) ? false : true;
}

NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  if (!NPN_IdentifierIsString(identifier)) {
    return NULL;
  }
  size_t length = strlen(static_cast<const char*>(identifier)) + 1;
  char* utf8 = static_cast<char*>(NPN_MemAlloc(length));
  if (utf8) {
    memmove(utf8, identifier, length);
  }
  return utf8;
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
  uintptr_t intid = reinterpret_cast<uintptr_t>(identifier);
  if (intid & 1) {
    return intid >> 1;
  } else {
    return -2147483647 - 1;  // PR_INT32_MIN
  }
  return 0;
}

NPObject* NPN_CreateObject(NPP npp, NPClass* aClass) {
  if (!npp || !aClass) {
    return NULL;
  }
  NPObject* object;
  if (aClass->allocate) {
    object = aClass->allocate(npp, aClass);
  } else {
    object = static_cast<NPObject*>(malloc(sizeof(NPObject)));
  }
  if (object) {
    object->_class = aClass;
    object->referenceCount = 1;
  }
  return object;
}

NPObject* NPN_RetainObject(NPObject* object) {
  if (object) {
    ++(object->referenceCount);
  }
  return object;
}

void NPN_ReleaseObject(NPObject* object) {
  if (!object) {
    return;
  }
  int32_t count = --(object->referenceCount);
  if (count == 0) {
    if (object->_class && object->_class->deallocate) {
      object->_class->deallocate(object);
    } else {
      free(object);
    }
  }
}

bool NPN_Invoke(NPP npp, NPObject* object, NPIdentifier methodName,
                const NPVariant* args, uint32_t argCount, NPVariant* result) {
  if (!npp || !object || !object->_class || !object->_class->invoke) {
    return false;
  }
  return object->_class->invoke(object, methodName, args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject* object, const NPVariant* args,
                       uint32_t argCount, NPVariant* result) {
  if (!npp || !object || !object->_class || !object->_class->invokeDefault) {
    return false;
  }
  return object->_class->invokeDefault(object, args, argCount, result);
}

bool NPN_GetProperty(NPP npp, NPObject* object, NPIdentifier propertyName,
                     NPVariant* result) {
  if (!npp || !object || !object->_class || !object->_class->getProperty) {
    return false;
  }
  return object->_class->getProperty(object, propertyName, result);
}

bool NPN_SetProperty(NPP npp, NPObject* object, NPIdentifier propertyName,
                     const NPVariant* value) {
  if (!npp || !object || !object->_class || !object->_class->setProperty) {
    return false;
  }
  return object->_class->setProperty(object, propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject* object, NPIdentifier propertyName) {
  if (!npp || !object || !object->_class || !object->_class->removeProperty) {
    return false;
  }
  return object->_class->removeProperty(object, propertyName);
}

#if 1 <= NP_VERSION_MAJOR || 19 <= NP_VERSION_MINOR
// The following two functions for NPObject are supported in the NPAPI
// version 0.19 and newer. Note currently we are using 0.18.

bool NPN_Enumerate(NPP npp, NPObject* object, NPIdentifier** identifier,
                   uint32_t* count) {
  if (!npp || !object || !object->_class) {
    return false;
  }
  if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(object->_class) ||
      !object->_class->enumerate) {
    *identifier = 0;
    *count = 0;
    return true;
  }
  return object->_class->enumerate(object, identifier, count);
}

bool NPN_Construct(NPP npp, NPObject* object, const NPVariant* args,
                   uint32_t argCount, NPVariant* result) {
  if (!npp || !object || !object->_class ||
      !NP_CLASS_STRUCT_VERSION_HAS_CTOR(object->_class) ||
      !object->_class->construct) {
    return false;
  }
  return object->_class->construct(object, args, argCount, result);
}

#endif

bool NPN_HasProperty(NPP npp, NPObject* object, NPIdentifier propertyName) {
  if (!npp || !object || !object->_class || !object->_class->hasProperty) {
    return false;
  }
  return object->_class->hasProperty(object, propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject* object, NPIdentifier methodName) {
  if (!npp || !object || !object->_class || !object->_class->hasMethod) {
    return false;
  }
  return object->_class->hasMethod(object, methodName);
}

void NPN_ReleaseVariantValue(NPVariant* variant) {
  switch (variant->type) {
  case NPVariantType_Void:
  case NPVariantType_Null:
  case NPVariantType_Bool:
  case NPVariantType_Int32:
  case NPVariantType_Double:
    break;
  case NPVariantType_String: {
    const NPString* string = &NPVARIANT_TO_STRING(*variant);
    NPN_MemFree(const_cast<NPUTF8*>(string->utf8characters));
    break;
  }
  case NPVariantType_Object: {
    NPObject* object = NPVARIANT_TO_OBJECT(*variant);
    NPN_ReleaseObject(object);
    break;
  }
  default:
    break;
  }
  VOID_TO_NPVARIANT(*variant);
}

void NPN_SetException(NPObject* object, const NPUTF8* message) {
  if (object == NULL) {
    return;
  }
  if (nacl::NPObjectProxy::IsInstance(object)) {
    nacl::NPObjectProxy* proxy = static_cast<nacl::NPObjectProxy*>(object);
    proxy->SetException(message);
    return;
  }
  nacl::NPNavigator* navigator = NaClNP_GetNavigator();
  if (navigator == NULL) {
    return;
  }
  nacl::NPObjectStub* stub = navigator->LookupStub(object);
  if (stub) {
    stub->SetException(message);
  }
}

void NPN_InvalidateRect(NPP instance, NPRect* invalid_rect) {
  if (instance == NULL) {
    return;
  }
  if (nacl::NPNavigator* navigator = NaClNP_GetNavigator()) {
    navigator->InvalidateRect(invalid_rect);
  }
}

void NPN_ForceRedraw(NPP instance) {
  if (instance == NULL) {
    return;
  }
  if (nacl::NPNavigator* navigator = NaClNP_GetNavigator()) {
    navigator->ForceRedraw();
  }
}
