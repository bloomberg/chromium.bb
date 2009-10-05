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


/**
  * @file
  * Defines the basic API for the nacl browser plugin
  *
  * http://www.mozilla.org/projects/plugins/
  * http://www.ssuet.edu.pk/taimoor/books/0-7897-1063-3/ch11.htm

  * NOTE: This is mostly a wrapper for module specific functions defined
  *       elsewhere
  * TODO: give some more details
  */

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// To define nothrow object to prevent exceptions from new.
#include <new>

#include "native_client/src/include/elf.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/trusted/plugin/srpc/srpc.h"
#include "native_client/src/trusted/service_runtime/expiration.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

#define EXPAND(x) x
#define ENQUOTE(x) #x
#define EMBED(x) EXPAND(ENQUOTE(x))

const char kPluginName[] = "Native Client Plugin";
const char kPluginDescription[] = "Native Client Plugin was built on "
                                  __DATE__ " at " __TIME__
#if defined(EXPIRATION_CHECK)
                                  " and expires on "
                                  EMBED(EXPIRATION_MONTH) "/"
                                  EMBED(EXPIRATION_DAY) "/"
                                  EMBED(EXPIRATION_YEAR)
                                  " (mm/dd/yyyy)"
#endif
                                  "";

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf(stderr, "@@@ NPAPI PLUGIN ");

  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

char* NPP_GetMIMEDescription() {
  return const_cast<char*>("application/x-nacl-srpc:nexe:NativeClient"
                           " Simple RPC module;");
}

// Invoked from NP_Initialize()
NPError NPP_Initialize() {
  DebugPrintf("NPP_Initialize\n");
  if (NaClHasExpired()) {
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }
  NaClNrdAllModulesInit();
  return NPERR_NO_ERROR;
}

// Invoked from NP_Shutdown()
void NPP_Shutdown() {
  NaClNrdAllModulesFini();
  DebugPrintf("NPP_Shutdown\n");
}

NPError NPP_New(NPMIMEType plugin_type,
                NPP instance,
                uint16_t mode,
                int16_t argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved) {
  UNREFERENCED_PARAMETER(mode);
  UNREFERENCED_PARAMETER(saved);
  DebugPrintf("NPP_New '%s'\n", plugin_type);

  for (int i = 0; i < argc; ++i) {
    DebugPrintf("args %u: '%s' '%s'\n", i, argn[i], argv[i]);
  }

  if (NaClHasExpired()) {
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  if (strcmp(plugin_type, "application/x-nacl-srpc") == 0) {
    instance->pdata = static_cast<nacl::NPInstance*>(
        new(std::nothrow) nacl::SRPC_Plugin(instance, argc, argn, argv));
  }
  if (instance->pdata == NULL) {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }
#ifndef NACL_STANDALONE
  // NaCl is a windowless plugin
  NPN_SetValue(instance, NPPVpluginWindowBool, false);
#endif
  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  DebugPrintf("NPP_Destroy\n");
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    instance->pdata = NULL;
    NPError nperr = module->Destroy(save);  // module must delete itself.
    return nperr;
  }
  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  DebugPrintf("NPP_SetWindow\n");
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  if (window == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    return module->SetWindow(window);
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  DebugPrintf("NPP_GetValue\n");
  if (variable == NPPVpluginNameString) {
    *static_cast<const char**>(value) = kPluginName;
    return NPERR_NO_ERROR;
  }
  if (variable == NPPVpluginDescriptionString) {
    *static_cast<const char**>(value) = kPluginDescription;
    return NPERR_NO_ERROR;
  }
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    return module->GetValue(variable, value);
  }
  return NPERR_GENERIC_ERROR;
}

int16_t NPP_HandleEvent(NPP instance, void* event) {
  DebugPrintf("NPP_HandleEvent\n");
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    return module->HandleEvent(event);
  }
  return 0;
}

// NOTE: This may never be called as there is an alternative:
//   NPP_GetValue(NPPVpluginScriptableNPObject)
NPObject* NPP_GetScriptableInstance(NPP instance) {
  DebugPrintf("NPP_GetScriptableInstance\n");
  if (instance == NULL) {
    return NULL;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    return module->GetScriptableInstance();
  }
  return NULL;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type,
                      NPStream* stream, NPBool seekable,
                      uint16_t* stype) {
  DebugPrintf("NPP_NewStream\n");
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    return module->NewStream(type, stream, seekable, stype);
  }
  return NPERR_GENERIC_ERROR;
}

int32_t NPP_WriteReady(NPP instance, NPStream* stream) {
  DebugPrintf("NPP_WriteReady \n");
  if ((NULL == instance) || (NULL == instance->pdata)) {
    return 0;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  return module->WriteReady(stream);
}

int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len,
                  void* buffer) {
  DebugPrintf("NPP_Write: offset %d, len %d\n", offset, len);
  if ((NULL == instance) || (NULL == instance->pdata)) {
    return -1;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  return module->Write(stream, offset, len, buffer);
}

#if NACL_OSX
static void OpenMacFile(NPStream* stream,
                        const char* filename,
                        nacl::NPInstance* module) {
  // This ugliness is necessary due to the fact that Safari on Mac returns
  // a pathname in "Mac" format, rather than a unix pathname.  To use the
  // resulting name requires conversion, which is done by a couple of Mac
  // library routines.
  // TODO(sehr): pull this code out into a separate file.
  if (filename && filename[0] != '/') {
    // The filename we were given is a "classic" pathname, which needs
    // to be converted to a posix pathname.
    Boolean got_posix_name = FALSE;
    CFStringRef cf_hfs_filename =
      CFStringCreateWithCString(NULL, filename, kCFStringEncodingMacRoman);
    if (cf_hfs_filename) {
      printf("Pathname after hfs\n");
      CFURLRef cf_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                      cf_hfs_filename,
                                                      kCFURLHFSPathStyle,
                                                      false);
      if (cf_url) {
        CFStringRef cf_posix_filename =
          CFURLCopyFileSystemPath(cf_url, kCFURLPOSIXPathStyle);
        if (cf_posix_filename) {
          CFIndex len
             = CFStringGetMaximumSizeOfFileSystemRepresentation(
                 cf_posix_filename);
          if (len > 0) {
            char *posix_filename =
              static_cast<char*>(malloc(sizeof(posix_filename[0]) * len));
            if (posix_filename) {
              got_posix_name =
                CFStringGetFileSystemRepresentation(cf_posix_filename,
                                                    posix_filename,
                                                    len);
              if (got_posix_name) {
                module->StreamAsFile(stream, posix_filename);
                // Safari on OS X apparently wants the NPP_StreamAsFile
                // call to delete the file object after processing.
                // This was discovered in investigations by Shiki.
                FSRef ref;
                Boolean is_dir;
                if (FSPathMakeRef(reinterpret_cast<UInt8*>(posix_filename),
                                  &ref,
                                  &is_dir) == noErr) {
                  FSDeleteObject(&ref);
                }
              }
              free(posix_filename);
            }
          }
          CFRelease(cf_posix_filename);
        }
        CFRelease(cf_url);
      }
      CFRelease(cf_hfs_filename);
    }
    if (got_posix_name) {
      // If got_posix_name was true than we succesfully created
      // our posix path and called StreamAsFile above, so we
      // can exit without falling through to the case below.
      return;
    }
    filename =  NULL;
  }
  module->StreamAsFile(stream, filename);
}
#endif  // NACL_OSX

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* filename) {
  DebugPrintf("NPP_StreamAsFile: %s\n", filename);
  if (instance == NULL) {
    return;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
#if NACL_OSX
    OpenMacFile(stream, filename, module);
#else
    module->StreamAsFile(stream, filename);
#endif  // NACL_OSX
  }
}

// Note Safari running on OS X removes the loaded file at this point.
// To keep the file, it must be opened in NPP_StreamAsFile().
NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason) {
  DebugPrintf("NPP_DestroyStream %d\n", reason);
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    return module->DestroyStream(stream, reason);
  }
  return NPERR_GENERIC_ERROR;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notify_data) {
  DebugPrintf("NPP_URLNotify(%s)\n", url);
  if (instance == NULL) {
    return;
  }
  nacl::NPInstance* module = static_cast<nacl::NPInstance*>(instance->pdata);
  if (module != NULL) {
    module->URLNotify(url, reason, notify_data);
  }
}
