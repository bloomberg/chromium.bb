// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#include "native_client/src/shared/npruntime/npn_gate.h"

#include <stdlib.h>

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npextensions.h"
#include "native_client/src/shared/npruntime/npnavigator.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"

using nacl::NPNavigator;
using nacl::NPObjectStub;
using nacl::NPObjectProxy;

namespace nacl {

const NPNetscapeFuncs* GetBrowserFuncs() {
  static const NPNetscapeFuncs kBrowserFuncs = {
    static_cast<uint16_t>(sizeof(NPNetscapeFuncs)),
    (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR,
    NPN_GetURL,
    NPN_PostURL,
    NPN_RequestRead,
    NPN_NewStream,
    NPN_Write,
    NPN_DestroyStream,
    NPN_Status,
    NPN_UserAgent,
    NPN_MemAlloc,
    NPN_MemFree,
    NPN_MemFlush,  // Always returns failure in Pepper
    NPN_ReloadPlugins,
    NULL,  // NPN_GetJavaEnv is not implemented in Pepper
    NULL,  // NPN_GetJavaPeer is not implemented in Pepper
    NPN_GetURLNotify,
    NPN_PostURLNotify,
    NPN_GetValue,
    NPN_SetValue,
    NULL,  // NPN_InvalidateRect is not implemented in Pepper
    NULL,  // NPN_InvalidateRegion is not implemented in Pepper
    NULL,  // NPN_ForceRedraw is not implemented in Pepper
    NPN_GetStringIdentifier,
    NPN_GetStringIdentifiers,
    NPN_GetIntIdentifier,
    NPN_IdentifierIsString,
    NPN_UTF8FromIdentifier,
    NPN_IntFromIdentifier,
    NPN_CreateObject,
    NPN_RetainObject,
    NPN_ReleaseObject,
    NPN_Invoke,
    NPN_InvokeDefault,
    NPN_Evaluate,
    NPN_GetProperty,
    NPN_SetProperty,
    NPN_RemoveProperty,
    NPN_HasProperty,
    NPN_HasMethod,
    NPN_ReleaseVariantValue,
    NPN_SetException,
    NULL,  // NPN_PushPopupsEnabledState is not implemented in Pepper
    NULL,  // NPN_PopPopupsEnabledState is not implemented in Pepper
    NPN_Enumerate,
    NPN_PluginThreadAsyncCall,
    NPN_Construct
  };

  return &kBrowserFuncs;
}

}  // namespace nacl

NPError NPN_GetURL(NPP instance,
                   const char* url,
                   const char* window) {
  if (NULL == instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  navigator->GetUrl(instance, url, window);
  return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURL(NPP instance,
                    const char* url,
                    const char* window,
                    uint32_t len,
                    const char* buf,
                    NPBool file) {
  // TODO(sehr): implement this.
  return NPERR_GENERIC_ERROR;
}

NPError NPN_RequestRead(NPStream* stream,
                        NPByteRange* rangeList) {
  // TODO(sehr): implement this.
  return NPERR_GENERIC_ERROR;
}

NPError NPN_NewStream(NPP instance,
                      NPMIMEType type,
                      const char* window,
                      NPStream** stream) {
  // TODO(sehr): implement this.
  return NPERR_GENERIC_ERROR;
}

int32_t NPN_Write(NPP instance,
                  NPStream* stream,
                  int32_t len,
                  void* buffer) {
  // TODO(sehr): implement this.
  return -1;
}

NPError NPN_DestroyStream(NPP instance,
                          NPStream* stream,
                          NPReason reason) {
  // TODO(sehr): implement this.
  return NPERR_GENERIC_ERROR;
}

void NPN_Status(NPP instance,
                const char* message) {
  if (NULL == instance || NULL == message) {
    return;
  }
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  navigator->SetStatus(instance, message);
}

const char* NPN_UserAgent(NPP instance) {
  if (NULL == instance) {
    return NULL;
  }
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->UserAgent(instance);
}

void* NPN_MemAlloc(uint32_t size) {
  return malloc(size);
}

void NPN_MemFree(void* ptr) {
  free(ptr);
}

uint32_t NPN_MemFlush(uint32_t size) {
  // Not implemented in Pepper.  Returns failure to free any memory.
  return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages) {
  // TODO(sehr): implement this.
}

NPError NPN_GetURLNotify(NPP instance,
                         const char* url,
                         const char* window,
                         void* notifyData) {
  if (NULL == instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  navigator->GetUrlNotify(instance, url, window, notifyData);
  return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURLNotify(NPP instance,
                          const char* url,
                          const char* window,
                          uint32_t len,
                          const char* buf,
                          NPBool file,
                          void* notifyData) {
  // TODO(sehr): implement this.
  return NPERR_GENERIC_ERROR;
}

NPError NPN_GetValue(NPP instance,
                     NPNVariable variable,
                     void* value) {
  if (NULL == instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  NPNavigator* navigator;
  switch (variable) {
    case NPNVjavascriptEnabledBool:
    case NPNVSupportsWindowless:
      *reinterpret_cast<NPBool*>(value) = TRUE;
      return NPERR_NO_ERROR;

    case NPNVSupportsXEmbedBool:
      *reinterpret_cast<NPBool*>(value) = FALSE;
      return NPERR_NO_ERROR;

    case NPNVisOfflineBool:
    case NPNVprivateModeBool:
      navigator = NPNavigator::GetNavigator();
      if (NULL == navigator) {
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      return navigator->GetValue(instance,
                                 variable,
                                 reinterpret_cast<NPBool*>(value));

    case NPNVWindowNPObject:
    case NPNVPluginElementNPObject:
      navigator = NPNavigator::GetNavigator();
      if (NULL == navigator) {
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      return navigator->GetValue(instance,
                                 variable,
                                 reinterpret_cast<NPObject**>(value));

    case NPNVPepperExtensions:
      *reinterpret_cast<struct NPExtensions**>(value) =
          const_cast<struct NPExtensions*>(nacl::GetNPExtensions());
      return NPERR_NO_ERROR;

    case NPNVxDisplay:
    case NPNVxtAppContext:
    case NPNVnetscapeWindow:
    case NPNVasdEnabledBool:
    case NPNVserviceManager:
    case NPNVDOMElement:
    case NPNVDOMWindow:
    case NPNVToolkit:
    default:
      return NPERR_INVALID_PARAM;
  }
}

NPError NPN_SetValue(NPP instance,
                     NPPVariable variable,
                     void* ret_value) {
  // TODO(sehr): implement this.
  return NPERR_GENERIC_ERROR;
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name) {
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->GetStringIdentifier(name);
}

void NPN_GetStringIdentifiers(const NPUTF8** names,
                              int32_t nameCount,
                              NPIdentifier* identifiers) {
  NPNavigator* navigator = NPNavigator::GetNavigator();
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
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->GetIntIdentifier(intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->IdentifierIsString(identifier);
}

NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->UTF8FromIdentifier(identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->IntFromIdentifier(identifier);
}

NPObject* NPN_CreateObject(NPP npp,
                           NPClass* aClass) {
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

bool NPN_Evaluate(NPP npp,
                  NPObject* object,
                  NPString* script,
                  NPVariant* result) {
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return false;
  }
  return navigator->Evaluate(npp, object, script, result);
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
    // If the object was a proxy, then this does not decrement the reference
    // count on the corresponding stub.
    // TODO(sehr): reference counting on proxied objects needs work.
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

bool NPN_RemoveProperty(NPP npp,
                        NPObject* object,
                        NPIdentifier propertyName) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->removeProperty) {
    return false;
  }
  return object->_class->removeProperty(object, propertyName);
}

bool NPN_HasProperty(NPP npp,
                     NPObject* object,
                     NPIdentifier propertyName) {
  if (NULL == npp ||
      NULL == object ||
      NULL == object->_class ||
      NULL == object->_class->hasProperty) {
    return false;
  }
  return object->_class->hasProperty(object, propertyName);
}

bool NPN_HasMethod(NPP npp,
                   NPObject* object,
                   NPIdentifier methodName) {
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
    const NPString* str = &NPVARIANT_TO_STRING(*variant);
    NPN_MemFree(const_cast<NPUTF8*>(str->UTF8Characters));
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

void NPN_SetException(NPObject* object,
                      const NPUTF8* message) {
  if (NULL == object) {
    return;
  }
  if (NPObjectProxy::IsInstance(object)) {
    NPObjectProxy* proxy = static_cast<NPObjectProxy*>(object);
    proxy->SetException(message);
    return;
  }
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  NPObjectStub* stub = NPObjectStub::GetByObject(object);
  if (stub) {
    stub->SetException(message);
  }
}

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

void NPN_PluginThreadAsyncCall(NPP instance,
                               void (*func)(void*),
                               void* user_data) {
  if (NULL == instance) {
    return;
  }
  NPNavigator* navigator = NPNavigator::GetNavigator();
  if (NULL == navigator) {
    return;
  }
  navigator->PluginThreadAsyncCall(instance, func, user_data);
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
