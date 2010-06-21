/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction - NPAPI implementation

#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npmodule.h"
#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

using nacl::assert_cast;

namespace {

// Create an NPString from a C string.
bool CopyToNPString(const nacl::string& string, NPString* npstr) {
  // We have to create a copy of the string using NPN_MemAlloc, because the
  // browser sometimes will free the string using NPN_MemFree. Strdup and
  // friends use malloc, which would mix the memory allocators.
  // The copy does not include the terminating NUL, as the NPString UTF8Length
  // member does not include the NUL.
  uint32_t len = nacl::saturate_cast<uint32_t>(string.length());
  char* copy = reinterpret_cast<char*>(NPN_MemAlloc(len));
  if (copy == NULL) {
    PLUGIN_PRINTF(("NPN_MemAlloc failed in CopyToNPString.\n"));
    return false;
  }
  memcpy(copy, string.c_str(), len);
  // Populate the NPString.
  npstr->UTF8Characters = reinterpret_cast<NPUTF8*>(copy);
  npstr->UTF8Length = len;
  return true;
}

void FreeNPString(NPString& npstr) {
  void* str_bytes =
      reinterpret_cast<void*>(const_cast<NPUTF8*>(npstr.UTF8Characters));
  NPN_MemFree(str_bytes);
}

}  // namespace

namespace plugin {

uintptr_t BrowserImplNpapi::StringToIdentifier(const nacl::string& str) {
  return reinterpret_cast<uintptr_t>(NPN_GetStringIdentifier(str.c_str()));
}

nacl::string BrowserImplNpapi::IdentifierToString(uintptr_t ident) {
  NPIdentifier npident = reinterpret_cast<NPIdentifier>(ident);
  // For NPAPI there is no method to test that an identifier is "valid"
  // in the sense that it was created by either NPN_GetStringIdentifier or
  // NPN_GetIntIdentifier.  There is a only test for whether it was created by
  // the former.  Therefore, we may report invalid identifiers as integer
  // identifiers, or on platforms where integer identifiers are checked, we
  // may fail.  (No such platform is known at this point.)
  if (NPN_IdentifierIsString(npident)) {
    return reinterpret_cast<char*>(NPN_UTF8FromIdentifier(npident));
  } else {
    char buf[10];
    SNPRINTF(buf, sizeof buf, "%d", NPN_IntFromIdentifier(npident));
    return buf;
  }
}

bool BrowserImplNpapi::EvalString(InstanceIdentifier instance_id,
                                  const nacl::string& expression) {
  NPP instance = instance_id;
  NPVariant dummy_return;

  VOID_TO_NPVARIANT(dummy_return);

  NPString str;
  if (!CopyToNPString(expression, &str)) {
    return false;
  }

  do {
    NPObject* element_obj;
    if (NPERR_NO_ERROR !=
        NPN_GetValue(instance, NPNVPluginElementNPObject, &element_obj)) {
      break;
    }
    if (!NPN_Evaluate(instance, element_obj, &str, &dummy_return)) {
      break;
    }
  } while (0);

  // Free the dummy return from the evaluate.
  NPN_ReleaseVariantValue(&dummy_return);
  // Free the string copy we allocated in CopyToNPString.
  FreeNPString(str);

  return true;
}

bool BrowserImplNpapi::Alert(InstanceIdentifier instance_id,
                             const nacl::string& text) {
  // Usually these messages are important enough to call attention to them.
  puts(text.c_str());

  NPObject* window;
  NPP npp = instance_id;
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

bool BrowserImplNpapi::GetOrigin(InstanceIdentifier instance_id,
                                 nacl::string *origin) {
  NPVariant loc_value;
  NPVariant href_value;

  *origin = "";

  VOID_TO_NPVARIANT(loc_value);
  VOID_TO_NPVARIANT(href_value);

  // TODO(gregoryd): consider making this block a function returning origin.
  do {
    NPObject *win_obj;  // TODO(adonovan): NPN_ReleaseObject(win_obj) needed?
    if (NPERR_NO_ERROR !=
        NPN_GetValue(instance_id, NPNVWindowNPObject, &win_obj)) {
        PLUGIN_PRINTF(("GetOrigin: No window object\n"));
        // no window; no URL as NaCl descriptors will be allowed
        break;
    }
    if (!NPN_GetProperty(instance_id,
                         win_obj,
                         PluginNpapi::kLocationIdent,
                         &loc_value)) {
        PLUGIN_PRINTF(("GetOrigin: no location property value\n"));
        break;
    }
    NPObject *loc_obj = NPVARIANT_TO_OBJECT(loc_value);

    if (!NPN_GetProperty(instance_id,
                         loc_obj,
                         PluginNpapi::kHrefIdent,
                         &href_value)) {
        PLUGIN_PRINTF(("GetOrigin: no href property value\n"));
        break;
    }
    *origin =
        nacl::string(NPVARIANT_TO_STRING(href_value).UTF8Characters,
                     NPVARIANT_TO_STRING(href_value).UTF8Length);
    PLUGIN_PRINTF(("GetOrigin: origin %s\n", origin->c_str()));

  } while (0);

  NPN_ReleaseVariantValue(&loc_value);
  NPN_ReleaseVariantValue(&href_value);

  return ("" != *origin);
}

// TODO(gregoryd): consider refactoring and moving the code to service_runtime
bool BrowserImplNpapi::MightBeElfExecutable(const nacl::string& filename,
                                            nacl::string* error) {
  char buf[EI_ABIVERSION + 1];  // (field offset from file beginning)
  FILE* fp = fopen(filename.c_str(), "rb");
  if (fp == NULL) {
    *error = "Load failed: cannot open local file for reading.";
    return false;
  }
  if (fread(buf, sizeof buf, 1, fp) != 1) {
    *error = "Load failed: file too short to be an ELF executable.";
    return false;
  }
  fclose(fp);
  return MightBeElfExecutable(buf, sizeof buf, error);
}

bool BrowserImplNpapi::MightBeElfExecutable(const char* buffer,
                                            size_t size,
                                            nacl::string* error) {
  if (size < EI_ABIVERSION + 1) {
    *error = "Load failed: file too short to be an ELF executable.";
    return false;
  }
  const char EI_MAG0123[4] = { 0x7f, 'E', 'L', 'F' };
  if (strncmp(buffer, EI_MAG0123, sizeof EI_MAG0123) != 0) {
    // This can happen if we read a 404 error page, for example.
    *error = "Load failed: bad magic number; not an ELF executable.";
    return false;
  }
  if (buffer[EI_ABIVERSION] != EF_NACL_ABIVERSION) {
    nacl::stringstream ss;
    ss << "Load failed: ABI version mismatch: expected " << EF_NACL_ABIVERSION
       << ", got " << (unsigned) buffer[EI_ABIVERSION] << ".";
    *error = ss.str();
    return false;
  }
  // Returns a zero-length string if there were no errors.
  *error = "";
  return true;
}

// Creates a browser scriptable handle for a given portable handle.
ScriptableHandle* BrowserImplNpapi::NewScriptableHandle(
    PortableHandle* handle) {
  return ScriptableImplNpapi::New(handle);
}

}  // namespace plugin
