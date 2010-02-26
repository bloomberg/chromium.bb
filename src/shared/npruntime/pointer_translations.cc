// Copyright 2010 Google Inc. All Rights Reserved.
// Author: sehr@google.com (David Sehr)

#include "native_client/src/shared/npruntime/pointer_translations.h"
#include <map>
#include <new>
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "native_client/src/shared/npruntime/wire_format.h"

namespace {


#ifdef __native_client__
typedef nacl::NPWireLocalCopy<NPP, int32_t> NPPTranslateType;
typedef nacl::NPWireTrivial<NPIdentifier, int32_t> NPIdentifierTranslateType;
#elif NACL_BUILD_SUBARCH == 64
typedef nacl::NPWireOwner<NPP, int32_t> NPPTranslateType;
typedef nacl::NPWireOwner<NPIdentifier, int32_t> NPIdentifierTranslateType;
#else
typedef nacl::NPWireTrivial<NPP, int32_t> NPPTranslateType;
typedef nacl::NPWireTrivial<NPIdentifier, int32_t> NPIdentifierTranslateType;
#endif  // __native_client__

NPPTranslateType* g_npp_translations;
NPIdentifierTranslateType* g_npidentifier_translations;

bool IsInitialized() {
  return (NULL != g_npp_translations) && (NULL != g_npidentifier_translations);
}

}  // namespace;

namespace nacl {

void WireFormatInit() {
  g_npp_translations = new(std::nothrow) NPPTranslateType;
  g_npidentifier_translations = new(std::nothrow) NPIdentifierTranslateType;
}

void WireFormatFini() {
  delete g_npp_translations;
  g_npp_translations = NULL;
  delete g_npidentifier_translations;
  g_npidentifier_translations = NULL;
}

NPP WireFormatToNPP(int32_t wire_npp) {
  if (!IsInitialized()) {
    return NULL;
  }
  return g_npp_translations->ToNative(wire_npp);
}

int32_t NPPToWireFormat(NPP npp) {
  if (!IsInitialized()) {
    return -1;
  }
  return g_npp_translations->ToWire(npp);
}

#ifdef __native_client__
void SetLocalNPP(int32_t wire_npp, NPP npp) {
  if (!IsInitialized()) {
    return;
  }
  g_npp_translations->SetLocalCopy(wire_npp, npp);
}
#endif  // __native_client__

NPIdentifier WireFormatToNPIdentifier(int32_t wire_id) {
  if (!IsInitialized()) {
    return NULL;
  }
  return g_npidentifier_translations->ToNative(wire_id);
}

int32_t NPIdentifierToWireFormat(NPIdentifier id) {
  if (!IsInitialized()) {
    return -1;
  }
  return g_npidentifier_translations->ToWire(id);
}

}  // namespace nacl
