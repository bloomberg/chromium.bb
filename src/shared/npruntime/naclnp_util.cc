// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface
// This directory is shared because it contains browser plugin side and
// untrusted NaCl module side components.

#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/include/nacl_string.h"
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
#endif  // __native_client__

namespace {

enum DebugState {
  DEBUG_STATE_NOT_CHECKED = 0,
  DEBUG_STATE_NOT_ENABLED,
  DEBUG_STATE_ENABLED
};

DebugState debug_output_enabled = DEBUG_STATE_NOT_CHECKED;

bool DebugOutputEnabled() {
  if (DEBUG_STATE_NOT_CHECKED == debug_output_enabled) {
    if (getenv("NACL_NPAPI_DEBUG")) {
      debug_output_enabled = DEBUG_STATE_ENABLED;
    } else {
      debug_output_enabled = DEBUG_STATE_NOT_ENABLED;
    }
  }
  return DEBUG_STATE_ENABLED == debug_output_enabled;
}

nacl::string FormatNPVariantInternal(const NPVariant* variant) {
  nacl::stringstream ss;
  if (NULL == variant) {
    ss << "NULL";
  } else if (NPVARIANT_IS_VOID(*variant)) {
    ss << "NPVariant(VOID)";
  } else if (NPVARIANT_IS_NULL(*variant)) {
    ss << "NPVariant(NULL)";
  } else if (NPVARIANT_IS_BOOLEAN(*variant)) {
    ss << "NPVariant(bool, "
       << (NPVARIANT_TO_BOOLEAN(*variant) ? "true" : "false")
       << ")";
  } else if (NPVARIANT_IS_INT32(*variant)) {
    ss << "NPVariant(int32_t, "
       << NPVARIANT_TO_INT32(*variant)
       << ")";
  } else if (NPVARIANT_IS_DOUBLE(*variant)) {
    ss <<  "NPVariant(double, " << NPVARIANT_TO_DOUBLE(*variant) << ")";
  } else if (NPVARIANT_IS_STRING(*variant)) {
    ss << "NPVariant(string, \"";
    NPString str = NPVARIANT_TO_STRING(*variant);
    if (0 != str.UTF8Length && NULL != str.UTF8Characters) {
      nacl::string s(str.UTF8Characters);
      ss << s.substr(0, str.UTF8Length);
    }
    ss << "\")";
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    ss << "NPVariant(object, "
       << NPVARIANT_TO_OBJECT(*variant)
       << ")";
  } else {
    ss << "NPVariant(BAD TYPE)";
  }
  return ss.str();
}

}  // namespace

namespace nacl {

static const size_t kFormatBufSize = 1024;

const char* FormatNPIdentifier(NPIdentifier ident) {
  static char buf[kFormatBufSize];
  buf[0] = 0;
  if (!DebugOutputEnabled()) {
    return buf;
  }
  nacl::string s("NPIdentifier(");
  if (NPN_IdentifierIsString(ident)) {
    const NPUTF8* name = NPN_UTF8FromIdentifier(ident);
    s += name;
    NPN_MemFree(const_cast<NPUTF8*>(name));
  } else {
    s += static_cast<int>(NPN_IntFromIdentifier(ident));
  }
  s += ")";
  strncpy(buf, s.c_str(), kFormatBufSize);
  buf[kFormatBufSize - 1] = 0;
  return buf;
}

const char* FormatNPVariant(const NPVariant* variant) {
  static char buf[kFormatBufSize];
  buf[0] = 0;
  if (!DebugOutputEnabled()) {
    return buf;
  }
  strncpy(buf, FormatNPVariantInternal(variant).c_str(), kFormatBufSize);
  buf[kFormatBufSize - 1] = 0;
  return buf;
}

const char* FormatNPVariantVector(const NPVariant* vect, uint32_t count) {
  static char buf[kFormatBufSize];
  buf[0] = 0;
  if (!DebugOutputEnabled()) {
    return buf;
  }
  nacl::stringstream ss;
  ss << "[";
  for (uint32_t i = 0; i < count; ++i) {
    ss << FormatNPVariantInternal(vect + i);
    if (i < count - 1) {
      ss << ", ";
    }
  }
  ss << "]";
  strncpy(buf, ss.str().c_str(), kFormatBufSize);
  buf[kFormatBufSize - 1] = 0;
  return buf;
}

void DebugPrintf(const char *fmt, ...) {
  if (!DebugOutputEnabled()) {
    return;
  }
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

}  // namespace nacl
