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


// Portable interface for browser interaction - NPAPI implementation

#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include "native_client/src/include/base/basictypes.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

bool PortablePluginInterface::identifiers_initialized = false;
int PortablePluginInterface::kConnectIdent;
int PortablePluginInterface::kHeightIdent;
int PortablePluginInterface::kHrefIdent;
int PortablePluginInterface::kLengthIdent;
int PortablePluginInterface::kLocationIdent;
int PortablePluginInterface::kMapIdent;
int PortablePluginInterface::kModuleReadyIdent;
int PortablePluginInterface::kNaClMultimediaBridgeIdent;
int PortablePluginInterface::kNullNpapiMethodIdent;
int PortablePluginInterface::kOnfailIdent;
int PortablePluginInterface::kOnloadIdent;
int PortablePluginInterface::kReadIdent;
int PortablePluginInterface::kSetCommandLogIdent;
int PortablePluginInterface::kShmFactoryIdent;
int PortablePluginInterface::kSignaturesIdent;
int PortablePluginInterface::kSrcIdent;
int PortablePluginInterface::kToStringIdent;
int PortablePluginInterface::kUrlAsNaClDescIdent;
int PortablePluginInterface::kValueOfIdent;
int PortablePluginInterface::kVideoUpdateModeIdent;
int PortablePluginInterface::kWidthIdent;
int PortablePluginInterface::kWriteIdent;

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


bool PortablePluginInterface::Alert(
    nacl_srpc::PluginIdentifier plugin_identifier,
    const char *text,
    int length) {
  NPObject* window;
  NPP npp = plugin_identifier;
  NPN_GetValue(npp, NPNVWindowNPObject, &window);
  NPString script;
  const char *command_prefix = "alert('";
  const char *command_postfix = "');";
  script.utf8length = strlen(command_prefix) + strlen(command_postfix) + length;
  char* buffer = reinterpret_cast<char*>(NPN_MemAlloc(script.utf8length));
  SNPRINTF(buffer,
           script.utf8length,
           "%s%s%s",
           command_prefix,
           text,
           command_postfix);
  script.utf8characters = buffer;
  script.utf8length = strlen(script.utf8characters);  // similar to -=2 ?
  NPVariant result;
  bool success = NPN_Evaluate(npp, window, &script, &result);
  NPN_ReleaseVariantValue(&result);
  NPN_MemFree(buffer);

  return success;
}


bool PortablePluginInterface::GetOrigin(
    nacl_srpc::PluginIdentifier plugin_identifier,
    std::string **origin) {
  NPP instance = plugin_identifier;
  NPVariant loc_value;
  NPVariant href_value;

  *origin = NULL;

  VOID_TO_NPVARIANT(loc_value);
  VOID_TO_NPVARIANT(href_value);

  // TODO(gregoryd): consider making this block a function returning origin.
  do {
    NPObject *win_obj;
    if (NPERR_NO_ERROR
      != NPN_GetValue(instance, NPNVWindowNPObject, &win_obj)) {
        dprintf(("GetOrigin: No window object\n"));
        // no window; no URL as NaCl descriptors will be allowed
        break;
    }

    if (!NPN_GetProperty(instance, win_obj,
      (NPIdentifier)PortablePluginInterface::kLocationIdent,
      &loc_value)) {
        dprintf(("GetOrigin: no location property value\n"));
        break;
    }
    NPObject *loc_obj = NPVARIANT_TO_OBJECT(loc_value);

    if (!NPN_GetProperty(instance, loc_obj,
      (NPIdentifier)PortablePluginInterface::kHrefIdent,
      &href_value)) {
        dprintf(("GetOrigin: no href property value\n"));
        break;
    }
    std::string *href = new(std::nothrow) std::string(
      NPVARIANT_TO_STRING(href_value).utf8characters,
      NPVARIANT_TO_STRING(href_value).utf8length);
    dprintf(("GetOrigin: href %s\n", href->c_str()));

    *origin = href;
  } while (0);

  NPN_ReleaseVariantValue(&loc_value);
  NPN_ReleaseVariantValue(&href_value);

  return (NULL != *origin);
}


void* PortablePluginInterface::BrowserAlloc(int size) {
  return NPN_MemAlloc(size);
}


void PortablePluginInterface::BrowserRelease(void* ptr) {
  NPN_MemFree(ptr);
}


const char* PortablePluginInterface::IdentToString(uintptr_t ident) {
  if (NPN_IdentifierIsString((NPIdentifier)ident)) {
    return reinterpret_cast<char*>(NPN_UTF8FromIdentifier((NPIdentifier)ident));
  } else {
    static char buf[10];
    SNPRINTF(buf,
             sizeof(buf),
             "%d",
             NPN_IntFromIdentifier((NPIdentifier)ident));
    return buf;
  }
}

bool PortablePluginInterface::CheckExecutableVersionCommon(
    nacl_srpc::PluginIdentifier instance,
    const char *version) {
  if ((NULL != version) && (EF_NACL_ABIVERSION == *version)) {
    return true;
  }
  char alert[256];
  if (NULL == version) {
    SNPRINTF(alert,
      sizeof alert,
      "alert('Load failed: Unknown error\\n');");
  } else {
    SNPRINTF(alert,
      sizeof alert,
      "alert('Load failed: ABI version mismatch: expected %d, got %d\\n');",
      EF_NACL_ABIVERSION,
      *version);
  }
  Alert(instance, alert, sizeof(alert));
  return false;
}

// TODO(gregoryd): consider refactoring and moving the code to service_runtime
bool PortablePluginInterface::CheckExecutableVersion(
    nacl_srpc::PluginIdentifier instance,
    const char *filename) {
  FILE *f;
  uint8_t nacl_abi_version = kInvalidAbiVersion;
  bool success = false;
  f = fopen(filename, "rb");
  if (NULL != f) {
    if (0 == fseek(f, EI_ABIVERSION, SEEK_SET)) {
      if (1 == fread(&nacl_abi_version, 1, 1, f)) {
        success = true;
      }
    }
    fclose(f);
  }

  if (success) {
    return CheckExecutableVersionCommon(
      instance,
      reinterpret_cast<char*>(&nacl_abi_version));
  } else {
    char alert[256];
    SNPRINTF(alert,
        sizeof alert,
        "alert('Load failed: Generic file error\\n');");
    Alert(instance, alert, sizeof(alert));
    return false;
  }
}

bool PortablePluginInterface::CheckExecutableVersion(
    nacl_srpc::PluginIdentifier instance,
    const void* buffer,
    int32_t size) {
  if ((EI_ABIVERSION + sizeof(char)) > static_cast<uint32_t>(size)) {
    return false;
  }

  const char *nacl_abi_version =
    reinterpret_cast<const char*>(buffer) + EI_ABIVERSION;
  return CheckExecutableVersionCommon(instance, nacl_abi_version);
}

char *PortablePluginInterface::MemAllocStrdup(const char *str) {
  int lenz = strlen(str) + 1;
  char *dup = static_cast<char *>(malloc(lenz));
  if (NULL != dup) {
    strncpy(dup, str, lenz);
  }
  // else abort();
  return dup;
}
