
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
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"

void* NPN_MemAlloc(uint32_t size) {
  return malloc(size);
}

void NPN_MemFree(void* ptr) {
  free(ptr);
}

void NPN_Status(NPP instance, const char* message) {
  if (NULL == instance || NULL == message) {
    return;
  }
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  navigator->SetStatus(instance, message);
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void* value) {
  if (NULL == instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  return navigator->GetValue(instance, variable, value);
  return NPERR_INVALID_PARAM;
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name) {
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->GetStringIdentifier(name);
}

void NPN_GetStringIdentifiers(const NPUTF8** names,
                              int32_t nameCount,
                              NPIdentifier* identifiers) {
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  for (int32_t i = 0; i < nameCount; ++i) {
    if (names[i]) {
      identifiers[i] = navigator->GetStringIdentifier(names[i]);
    } else {
      identifiers[i] = NULL;
    }
  }
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid) {
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->GetIntIdentifier(intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->IdentifierIsString(identifier);
}

NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->UTF8FromIdentifier(identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->IntFromIdentifier(identifier);
}

NPObject* NPN_CreateObject(NPP npp, NPClass* aClass) {
  if (NULL == npp || NULL == aClass) {
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
  if (NULL == object) {
    return;
  }
  int32_t count = --(object->referenceCount);
  if (0 == count) {
    if (object->_class && object->_class->deallocate) {
      object->_class->deallocate(object);
    } else {
      free(object);
    }
  }
}

bool NPN_Invoke(NPP npp,
                NPObject* object,
                NPIdentifier methodName,
                const NPVariant* args,
                uint32_t argCount,
                NPVariant* result) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->invoke) {
    return false;
  }
  return object->_class->invoke(object, methodName, args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp,
                       NPObject* object,
                       const NPVariant* args,
                       uint32_t argCount,
                       NPVariant* result) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->invokeDefault) {
    return false;
  }
  return object->_class->invokeDefault(object, args, argCount, result);
}

bool NPN_GetProperty(NPP npp,
                     NPObject* object,
                     NPIdentifier propertyName,
                     NPVariant* result) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->getProperty) {
    return false;
  }
  return object->_class->getProperty(object, propertyName, result);
}

bool NPN_SetProperty(NPP npp,
                     NPObject* object,
                     NPIdentifier propertyName,
                     const NPVariant* value) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->setProperty) {
    return false;
  }
  return object->_class->setProperty(object, propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject* object, NPIdentifier propertyName) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->removeProperty) {
    return false;
  }
  return object->_class->removeProperty(object, propertyName);
}

#if 1 <= NP_VERSION_MAJOR || 19 <= NP_VERSION_MINOR
// The following two functions for NPObject are supported in the NPAPI
// version 0.19 and newer. Note currently we are using 0.18.

bool NPN_Enumerate(NPP npp,
                   NPObject* object,
                   NPIdentifier** identifier,
                   uint32_t* count) {
  if (NULL == npp || NULL == object || NULL == object->_class) {
    return false;
  }
  if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(object->_class) ||
      NULL == object->_class->enumerate) {
    *identifier = 0;
    *count = 0;
    return true;
  }
  return object->_class->enumerate(object, identifier, count);
}

bool NPN_Construct(NPP npp,
                   NPObject* object,
                   const NPVariant* args,
                   uint32_t argCount,
                   NPVariant* result) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      !NP_CLASS_STRUCT_VERSION_HAS_CTOR(object->_class) ||
      NULL == object->_class->construct) {
    return false;
  }
  return object->_class->construct(object, args, argCount, result);
}

#endif

bool NPN_HasProperty(NPP npp, NPObject* object, NPIdentifier propertyName) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->hasProperty) {
    return false;
  }
  return object->_class->hasProperty(object, propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject* object, NPIdentifier methodName) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->hasMethod) {
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
  if (NULL == object) {
    return;
  }
  if (nacl::NPObjectProxy::IsInstance(object)) {
    nacl::NPObjectProxy* proxy = static_cast<nacl::NPObjectProxy*>(object);
    proxy->SetException(message);
    return;
  }
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  nacl::NPObjectStub* stub = navigator->LookupStub(object);
  if (stub) {
    stub->SetExceptionImpl(message);
  }
}

void NPN_InvalidateRect(NPP instance, NPRect* invalid_rect) {
  if (NULL == instance) {
    return;
  }
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  navigator->InvalidateRect(instance, invalid_rect);
}

void NPN_ForceRedraw(NPP instance) {
  if (NULL == instance) {
    return;
  }
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  navigator->ForceRedraw(instance);
}

void NPN_PluginThreadAsyncCall(NPP instance,
                               void (*func)(void*),
                               void* user_data) {
  if (NULL == instance) {
    return;
  }
  nacl::NPNavigator* navigator = nacl::NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  navigator->PluginThreadAsyncCall(instance, func, user_data);
}
