/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction - NPAPI implementation

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

using nacl::assert_cast;

bool PortablePluginInterface::identifiers_initialized = false;
uintptr_t PortablePluginInterface::kConnectIdent;
uintptr_t PortablePluginInterface::kHeightIdent;
uintptr_t PortablePluginInterface::kHrefIdent;
uintptr_t PortablePluginInterface::kLengthIdent;
uintptr_t PortablePluginInterface::kLocationIdent;
uintptr_t PortablePluginInterface::kMapIdent;
uintptr_t PortablePluginInterface::kModuleReadyIdent;
uintptr_t PortablePluginInterface::kNaClMultimediaBridgeIdent;
uintptr_t PortablePluginInterface::kNullNpapiMethodIdent;
uintptr_t PortablePluginInterface::kOnfailIdent;
uintptr_t PortablePluginInterface::kOnloadIdent;
uintptr_t PortablePluginInterface::kReadIdent;
uintptr_t PortablePluginInterface::kSetCommandLogIdent;
uintptr_t PortablePluginInterface::kShmFactoryIdent;
uintptr_t PortablePluginInterface::kSignaturesIdent;
uintptr_t PortablePluginInterface::kSrcIdent;
uintptr_t PortablePluginInterface::kToStringIdent;
uintptr_t PortablePluginInterface::kUrlAsNaClDescIdent;
uintptr_t PortablePluginInterface::kValueOfIdent;
uintptr_t PortablePluginInterface::kVideoUpdateModeIdent;
uintptr_t PortablePluginInterface::kWidthIdent;
uintptr_t PortablePluginInterface::kWriteIdent;

uint8_t const PortablePluginInterface::kInvalidAbiVersion = UINT8_MAX;

// TODO(gregoryd): make sure that calls to AddMethodToMap use the same strings
// move the strings to a header file.
void PortablePluginInterface::InitializeIdentifiers() {
  if (!identifiers_initialized) {
    kConnectIdent         = GetStrIdentifierCallback("connect");
    kHeightIdent          = GetStrIdentifierCallback("height");
    kHrefIdent            = GetStrIdentifierCallback("href");
    kLengthIdent          = GetStrIdentifierCallback("length");
    kLocationIdent        = GetStrIdentifierCallback("location");
    kMapIdent             = GetStrIdentifierCallback("map");
    kModuleReadyIdent     = GetStrIdentifierCallback("__moduleReady");
    kNaClMultimediaBridgeIdent =
        GetStrIdentifierCallback("nacl_multimedia_bridge");
    kNullNpapiMethodIdent = GetStrIdentifierCallback("__nullNpapiMethod");
    kOnfailIdent          = GetStrIdentifierCallback("onfail");
    kOnloadIdent          = GetStrIdentifierCallback("onload");
    kReadIdent            = GetStrIdentifierCallback("read");
    kSetCommandLogIdent   = GetStrIdentifierCallback("__setCommandLog");
    kShmFactoryIdent      = GetStrIdentifierCallback("__shmFactory");
    kSignaturesIdent      = GetStrIdentifierCallback("__signatures");
    kSrcIdent             = GetStrIdentifierCallback("src");
    kToStringIdent        = GetStrIdentifierCallback("toString");
    kUrlAsNaClDescIdent   = GetStrIdentifierCallback("__urlAsNaClDesc");
    kValueOfIdent         = GetStrIdentifierCallback("valueOf");
    kVideoUpdateModeIdent = GetStrIdentifierCallback("videoUpdateMode");
    kWidthIdent           = GetStrIdentifierCallback("width");
    kWriteIdent           = GetStrIdentifierCallback("write");

    identifiers_initialized = true;
  }
}


uintptr_t PortablePluginInterface::GetStrIdentifierCallback(
    const char *method_name) {
  return reinterpret_cast<uintptr_t>(NPN_GetStringIdentifier(method_name));
}

bool PortablePluginInterface::Alert(const nacl::string& text) {
  // Usually these messages are important enough to call attention to them.
  puts(text.c_str());

  NPObject* window;
  NPP npp = GetPluginIdentifier();
  if (NPN_GetValue(npp, NPNVWindowNPObject, &window) != NPERR_NO_ERROR) {
    return false;
  }

  NPVariant message;  // doesn't hold its own string data, so don't release
  STRINGN_TO_NPVARIANT(text.c_str(),
                       static_cast<uint32_t>(text.size()),
                       message);

  NPVariant result;
  VOID_TO_NPVARIANT(result);
  bool ok = NPN_Invoke(npp, window, NPN_GetStringIdentifier("alert"),
                       &message, 1, &result);
  if (ok) NPN_ReleaseVariantValue(&result);
  // TODO(adonovan): NPN_ReleaseObject(window) needed?
  return ok;
}

bool PortablePluginInterface::GetOrigin(nacl::string **origin) {
  NPP instance = GetPluginIdentifier();
  NPVariant loc_value;
  NPVariant href_value;

  *origin = NULL;

  VOID_TO_NPVARIANT(loc_value);
  VOID_TO_NPVARIANT(href_value);

  // TODO(gregoryd): consider making this block a function returning origin.
  do {
    NPObject *win_obj;  // TODO(adonovan): NPN_ReleaseObject(win_obj) needed?
    if (NPERR_NO_ERROR
      != NPN_GetValue(instance, NPNVWindowNPObject, &win_obj)) {
        dprintf(("GetOrigin: No window object\n"));
        // no window; no URL as NaCl descriptors will be allowed
        break;
    }
    if (!NPN_GetProperty(instance,
                         win_obj,
                         reinterpret_cast<NPIdentifier>(kLocationIdent),
                         &loc_value)) {
        dprintf(("GetOrigin: no location property value\n"));
        break;
    }
    NPObject *loc_obj = NPVARIANT_TO_OBJECT(loc_value);

    if (!NPN_GetProperty(instance,
                         loc_obj,
                         reinterpret_cast<NPIdentifier>(kHrefIdent),
                         &href_value)) {
        dprintf(("GetOrigin: no href property value\n"));
        break;
    }
    nacl::string *href = new(std::nothrow) nacl::string(
      NPVARIANT_TO_STRING(href_value).UTF8Characters,
      NPVARIANT_TO_STRING(href_value).UTF8Length);
    dprintf(("GetOrigin: href %s\n", href->c_str()));

    *origin = href;
  } while (0);

  NPN_ReleaseVariantValue(&loc_value);
  NPN_ReleaseVariantValue(&href_value);

  return (NULL != *origin);
}

namespace {

bool RunHandler(
    nacl_srpc::PluginIdentifier plugin_identifier,
    char* handler_string) {
  NPP instance = plugin_identifier;
  NPVariant dummy_return;

  VOID_TO_NPVARIANT(dummy_return);

  if (NULL == handler_string) {
    return false;
  }

  // Create an NPString for the handler value passed in.
  // We have to create a copy of the string, because the browser sometimes
  // will free the string.  The copy does not include the terminating NUL,
  // as the NPString UTF8Length member does not include the NUL.
  uint32_t handler_len = nacl::saturate_cast<uint32_t>(strlen(handler_string));
  // TODO(adonovan): any reason not to use strdup?  Or even some
  // higher-level function for populating an NPString?
  char* handler_copy = reinterpret_cast<char*>(NPN_MemAlloc(handler_len));
  if (handler_copy == NULL) {
    dprintf(("NPN_MemAlloc failed in RunHandler.\n"));
    return false;
  }
  memcpy(handler_copy, handler_string, handler_len);
  NPString str;
  str.UTF8Characters = reinterpret_cast<NPUTF8*>(handler_copy);
  str.UTF8Length = handler_len;

  do {
    NPObject* element_obj;
    if (NPERR_NO_ERROR !=
        NPN_GetValue(instance, NPNVPluginElementNPObject, &element_obj)) {
      break;
    }
    if (!NPN_Evaluate(instance,
                      element_obj,
                      &str,
                      &dummy_return)) {
      break;
    }
  } while (0);

  NPN_ReleaseVariantValue(&dummy_return);

  return true;
}

}  // namespace

char* PortablePluginInterface::LookupArgument(const char *key) {
  char **keys = argn();
  for (int ii = 0, len = argc(); ii < len; ++ii) {
    if (!strcmp(keys[ii], key)) {
      return argv()[ii];
    }
  }
  return NULL;
}

bool PortablePluginInterface::RunOnloadHandler() {
  return RunHandler(GetPluginIdentifier(), LookupArgument("onload"));
}

bool PortablePluginInterface::RunOnfailHandler() {
  return RunHandler(GetPluginIdentifier(), LookupArgument("onfail"));
}

void* PortablePluginInterface::BrowserAlloc(size_t size) {
  return NPN_MemAlloc(assert_cast<uint32_t>(size));
}

void PortablePluginInterface::BrowserRelease(void* ptr) {
  NPN_MemFree(ptr);
}


nacl::string PortablePluginInterface::IdentToString(uintptr_t ident) {
  NPIdentifier npident = reinterpret_cast<NPIdentifier>(ident);
  if (NPN_IdentifierIsString(npident)) {
    return reinterpret_cast<char*>(NPN_UTF8FromIdentifier(npident));
  } else {
    char buf[10];
    SNPRINTF(buf, sizeof buf, "%d", NPN_IntFromIdentifier(npident));
    return buf;
  }
}

// TODO(gregoryd): consider refactoring and moving the code to service_runtime
bool PortablePluginInterface::CheckElfExecutable(const char *filename) {
  char buf[EI_ABIVERSION + 1];  // (field offset from file beginning)
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
    Alert("Load failed: cannot open local file for reading.");
    return false;
  }
  if (fread(buf, sizeof buf, 1, fp) != 1) {
    Alert("Load failed: file too short to be an ELF executable.");
    return false;
  }
  fclose(fp);
  return CheckElfExecutable(buf, sizeof buf);
}

bool PortablePluginInterface::CheckElfExecutable(const char* buffer,
                                                 size_t size) {
  if (size < EI_ABIVERSION + 1) {
    Alert("Load failed: file too short to be an ELF executable.");
    return false;
  }
  const char EI_MAG0123[4] = { 0x7f, 'E', 'L', 'F' };
  if (strncmp(buffer, EI_MAG0123, sizeof EI_MAG0123) != 0) {
    // This can happen if we read a 404 error page, for example.
    Alert("Load failed: bad magic number; not an ELF executable.");
    return false;
  }
  if (buffer[EI_ABIVERSION] != EF_NACL_ABIVERSION) {
    nacl::stringstream ss;
    ss << "Load failed: ABI version mismatch: expected " << EF_NACL_ABIVERSION
       << ", got " << (unsigned) buffer[EI_ABIVERSION] << ".";
    Alert(ss.str());
    return false;
  }
  return true;
}

char *PortablePluginInterface::MemAllocStrdup(const char *str) {
  // TODO(adonovan): rename this function since it doesn't use MemAlloc.
  return strdup(str);
}
