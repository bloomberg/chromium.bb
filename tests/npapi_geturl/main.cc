// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include <nacl/nacl_npapi.h>
#include <nacl/npapi.h>
#include <nacl/npruntime.h>
#include <nacl/npupp.h>
#include <sys/stat.h>
#include "native_client/tests/npapi_runtime/plugin.h"

NPNetscapeFuncs* browser;

// Create a new instance of a plugin.
NPError NPP_New(NPMIMEType mime_type,
                NPP instance,
                uint16_t mode,
                int16_t argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  Plugin *plugin = Plugin::New(instance, mime_type, argc, argn, argv);
  if (NULL == plugin) {
    return NPERR_GENERIC_ERROR;
  }
  instance->pdata = reinterpret_cast<void*>(plugin);
  return NPERR_NO_ERROR;
}

// Destroy an instance of a plugin.
NPError NPP_Destroy(NPP instance,
                    NPSavedData** save) {
  if (NULL == instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  delete reinterpret_cast<Plugin*>(instance->pdata);
  return NPERR_NO_ERROR;
}

NPError NPP_GetValue(NPP instance,
                     NPPVariable variable,
                     void* ret_value) {
  if (NPPVpluginScriptableNPObject == variable) {
    if (NULL == instance || NULL == instance->pdata) {
      return NPERR_GENERIC_ERROR;
    }
    *reinterpret_cast<NPObject**>(ret_value) =
        NPN_RetainObject(reinterpret_cast<NPObject*>(instance->pdata));
    return NPERR_NO_ERROR;
  }
  return NPERR_INVALID_PARAM;
}

NPError NPP_SetWindow(NPP instance,
                      NPWindow* window) {
  return NPERR_NO_ERROR;
}

namespace {

static void ReportResult(NPP npp,
                         const char* fname,
                         char* string,
                         bool success) {
  // Get window.
  NPObject* window_object;
  NPError error = NPN_GetValue(npp, NPNVWindowNPObject, &window_object);
  if (error != NPERR_NO_ERROR) {
    return;
  }

  // Invoke the function.
  NPVariant args[3];
  uint32_t arg_count = sizeof(args) / sizeof(args[0]);
  STRINGZ_TO_NPVARIANT(fname, args[0]);
  STRINGZ_TO_NPVARIANT(string, args[1]);
  BOOLEAN_TO_NPVARIANT(success, args[2]);
  NPVariant retval;
  VOID_TO_NPVARIANT(retval);
  NPIdentifier report_result_id = NPN_GetStringIdentifier("ReportResult");
  if (!NPN_Invoke(npp,
                  window_object,
                  report_result_id,
                  args,
                  arg_count,
                  &retval)) {
    NPN_ReleaseVariantValue(&retval);
  }
}

}  // namespace

void NPP_StreamAsFile(NPP instance,
                      NPStream* stream,
                      const char* fname) {
  int fd = *reinterpret_cast<const int*>(stream);
  struct stat stb;
  FILE* iob = NULL;
  char* buf = NULL;
  size_t size = 0;
  size_t nchar = 0;
  int ch;

  if (-1 == fd) {
    char* str = strdup("Bad file descriptor received\n");
    ReportResult(instance, fname, str, false);
    return;
  }
  if (-1 == fstat(fd, &stb)) {
    char* str = strdup("fstat failed\n");
    ReportResult(instance, fname, str, false);
    return;
  }
  size = stb.st_size;
  iob = fdopen(fd, "r");
  if (NULL == iob) {
    printf("fdopen failed");
    return;
  }
  buf = reinterpret_cast<char*>(NPN_MemAlloc(size));
  for (nchar = 0; EOF != (ch = getc(iob)) && nchar < size - 1; ++nchar) {
    buf[nchar] = ch;
  }
  buf[nchar] = '\0';
  ReportResult(instance, fname, buf, true);
  fclose(iob);
}

// NP_Initialize
NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                      NPPluginFuncs* plugin_funcs) {
  // Imported APIs from the browser.
  browser = browser_funcs;
  // Exported APIs from the plugin.
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->getvalue = NPP_GetValue;
  plugin_funcs->asfile = NPP_StreamAsFile;
  return NPERR_NO_ERROR;
}
