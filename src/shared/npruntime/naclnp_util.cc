// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface
// This directory is shared because it contains browser plugin side and
// untrusted NaCl module side components.

#include <stdio.h>

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#ifdef __native_client__
#include "native_client/src/shared/npruntime/npnavigator.h"
#endif  // __native_client__

#ifdef __native_client__
NPObject* NaClNPN_CreateArray(NPP npp) {
  nacl::NPNavigator* navigator = static_cast<nacl::NPNavigator*>(npp->ndata);
  if (NULL == navigator) {
    return NULL;
  }
  return navigator->CreateArray(npp);
}

NPError NaClNPN_OpenURL(NPP npp,
                        const char* url,
                        void* notify_data,
                        void (*notify)(const char* url,
                                       void* notify_data,
                                       NaClSrpcImcDescType handle)) {
  nacl::NPNavigator* navigator = static_cast<nacl::NPNavigator*>(npp->ndata);
  if (NULL == navigator) {
    return NPERR_GENERIC_ERROR;
  }
  return navigator->OpenURL(npp, url, notify_data, notify);
}
#endif  // __native_client__

namespace nacl {

void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
#ifdef __native_client__
  fprintf(stderr, "@@@ NACL ");
#else
  fprintf(stderr, "@@@ HOST ");
#endif  // __native_client__
  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

void PrintIdent(NPIdentifier ident) {
  if (NPN_IdentifierIsString(ident)) {
    const NPUTF8* name = NPN_UTF8FromIdentifier(ident);
    printf("NPIdentifier(%s)", name);
    NPN_MemFree(const_cast<NPUTF8*>(name));
  } else {
    printf("NPIdentifier(%d)", static_cast<int>(NPN_IntFromIdentifier(ident)));
  }
}

void PrintVariant(const NPVariant* variant) {
  if (NULL == variant) {
    printf("NULL");
  } else if (NPVARIANT_IS_VOID(*variant)) {
    printf("NPVariant(VOID)");
  } else if (NPVARIANT_IS_NULL(*variant)) {
    printf("NPVariant(NULL)");
  } else if (NPVARIANT_IS_BOOLEAN(*variant)) {
    printf("NPVariant(bool, %s)",
           NPVARIANT_TO_BOOLEAN(*variant) ? "true" : "false");
  } else if (NPVARIANT_IS_INT32(*variant)) {
    printf("NPVariant(int32_t, %d)",
           static_cast<int>(NPVARIANT_TO_INT32(*variant)));
  } else if (NPVARIANT_IS_DOUBLE(*variant)) {
    printf("NPVariant(double, %f)", NPVARIANT_TO_DOUBLE(*variant));
  } else if (NPVARIANT_IS_STRING(*variant)) {
    NPString str = NPVARIANT_TO_STRING(*variant);
    printf("NPVariant(string, \"%*s\")",
           static_cast<int>(str.UTF8Length),
           str.UTF8Characters);
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    printf("NPVariant(object, %p)",
           reinterpret_cast<void*>(NPVARIANT_TO_OBJECT(*variant)));
  } else {
    printf("NPVariant(BAD TYPE)");
  }
}

}  // namespace nacl
