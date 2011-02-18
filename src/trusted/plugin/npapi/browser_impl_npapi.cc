/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction - NPAPI implementation

#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"

#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/plugin/npapi/plugin_npapi.h"
#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

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

void FreeNPString(NPString* npstr) {
  void* str_bytes =
      reinterpret_cast<void*>(const_cast<NPUTF8*>(npstr->UTF8Characters));
  NPN_MemFree(str_bytes);
}

bool LogToConsole(NPP npp,
                  NPVariant* console_variant,
                  const nacl::string& text) {
  NPObject* console_object = NPVARIANT_TO_OBJECT(*console_variant);
  NPVariant message;  // doesn't hold its own string data, so don't release
  bool success = false;
  STRINGN_TO_NPVARIANT(text.c_str(),
                       static_cast<uint32_t>(text.size()),
                       message);

  NPVariant result;
  VOID_TO_NPVARIANT(result);
  success = NPN_Invoke(npp, console_object, NPN_GetStringIdentifier("log"),
                       &message, 1, &result);
  if (success) {
    NPN_ReleaseVariantValue(&result);
  }
  return success;
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
  NPP instance = InstanceIdentifierToNPP(instance_id);
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
  FreeNPString(&str);

  return true;
}

bool BrowserImplNpapi::AddToConsole(InstanceIdentifier instance_id,
                                    const nacl::string& text) {
#if defined(NACL_STANDALONE)
  // We cannot be sure whether console.log is supported by the browser,
  // so we use alerts instead.
  return Alert(instance_id, text);
#else
  bool success = false;
  // Usually these messages are important enough to call attention to them.
  puts(text.c_str());

  NPObject* window = NULL;
  NPP npp = InstanceIdentifierToNPP(instance_id);
  if (NPN_GetValue(npp, NPNVWindowNPObject, &window) != NPERR_NO_ERROR) {
    return false;
  }

  NPVariant console_variant;
  if (!NPN_GetProperty(npp, window, NPN_GetStringIdentifier("console"),
                       &console_variant)) {
    goto cleanup_window;
  }
  if (!NPVARIANT_IS_OBJECT(console_variant)) {
    goto cleanup_console_variant;
  }

  success = LogToConsole(npp, &console_variant, text);

 cleanup_console_variant:
  NPN_ReleaseVariantValue(&console_variant);
 cleanup_window:
  NPN_ReleaseObject(window);
  return success;
#endif
}

bool BrowserImplNpapi::Alert(InstanceIdentifier instance_id,
                             const nacl::string& text) {
  // Usually these messages are important enough to call attention to them.
  puts(text.c_str());

  NPObject* window;
  NPP npp = InstanceIdentifierToNPP(instance_id);
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
  NPN_ReleaseObject(window);
  return ok;
}

bool BrowserImplNpapi::GetFullURL(InstanceIdentifier instance_id,
                                  nacl::string* full_url) {
  NPP npp = InstanceIdentifierToNPP(instance_id);
  NPObject* win_obj = NULL;
  NPVariant loc_value;
  NPVariant href_value;

  *full_url = NACL_NO_URL;

  VOID_TO_NPVARIANT(loc_value);
  VOID_TO_NPVARIANT(href_value);

  do {
    if (NPERR_NO_ERROR !=
        NPN_GetValue(npp, NPNVWindowNPObject, &win_obj)) {
        PLUGIN_PRINTF(("GetOrigin: No window object\n"));
        // no window; no URL as NaCl descriptors will be allowed
        break;
    }
    if (!NPN_GetProperty(npp,
                         win_obj,
                         PluginNpapi::kLocationIdent,
                         &loc_value)) {
        PLUGIN_PRINTF(("GetOrigin: no location property value\n"));
        break;
    }
    NPObject* loc_obj = NPVARIANT_TO_OBJECT(loc_value);

    if (!NPN_GetProperty(npp,
                         loc_obj,
                         PluginNpapi::kHrefIdent,
                         &href_value)) {
        PLUGIN_PRINTF(("GetOrigin: no href property value\n"));
        break;
    }
    *full_url = nacl::string(NPVARIANT_TO_STRING(href_value).UTF8Characters,
                             NPVARIANT_TO_STRING(href_value).UTF8Length);
    PLUGIN_PRINTF(("GetFullURL: full_url %s\n", full_url->c_str()));
  } while (0);

  if (win_obj != NULL) {
    NPN_ReleaseObject(win_obj);
  }
  NPN_ReleaseVariantValue(&loc_value);
  NPN_ReleaseVariantValue(&href_value);

  return (NACL_NO_URL != *full_url);
}

// Creates a browser scriptable handle for a given portable handle.
ScriptableHandle* BrowserImplNpapi::NewScriptableHandle(
    PortableHandle* handle) {
  return ScriptableImplNpapi::New(handle);
}

NPP InstanceIdentifierToNPP(InstanceIdentifier id) {
  return reinterpret_cast<NPP>(assert_cast<intptr_t>(id));
}

InstanceIdentifier NPPToInstanceIdentifier(NPP npp) {
  return assert_cast<InstanceIdentifier>(reinterpret_cast<intptr_t>(npp));
}

}  // namespace plugin
