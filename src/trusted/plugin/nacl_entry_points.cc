/*
* Copyright 2009, Google Inc.
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

#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "webkit/glue/plugins/plugin_list.h"

// This code inside the ifdef is copied from
// native_client/src/third_party_mod/npapi_plugin/np_entry.cc
// This is required as some function declarations are missing for Mac OSX
// in native_client/src/third_party/npapi/files/include/npupp.h that is included
// above.

extern "C" {
  // Safari under OS X requires the following three entry points to be exported.
  NPError API_CALL NP_Initialize(NPNetscapeFuncs* pFuncs
#if defined(XP_UNIX) && !defined(XP_MACOSX)
                                          , NPPluginFuncs* pluginFuncs
#endif  // XP_UNIX
    );
  NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* pFuncs);
  NPError API_CALL NP_Shutdown(void);
}

LaunchNaClProcessFunc launch_nacl_process = NULL;

NPError API_CALL NaCl_NP_GetEntryPoints(NPPluginFuncs* pFuncs) {
#if NACL_WINDOWS || NACL_OSX
  return NP_GetEntryPoints(pFuncs);
#else
  return 0;
#endif
}

NPError API_CALL NaCl_NP_Initialize(NPNetscapeFuncs* pFuncs
#if defined(XP_UNIX) && !defined(XP_MACOSX)
                                  , NPPluginFuncs* pluginFuncs
#endif  // NACL_LINUX
                                  ) {
                                    return NP_Initialize(pFuncs
#if defined(XP_UNIX) && !defined(XP_MACOSX)
                                      , pluginFuncs
#endif  // NACL_LINUX
                                      );
}

NPError API_CALL NaCl_NP_Shutdown() {
  return NP_Shutdown();
}

void RegisterInternalNaClPlugin(LaunchNaClProcessFunc launch_func) {
  NPAPI::PluginEntryPoints entry_points = {
#if !defined(OS_LINUX)
      NaCl_NP_GetEntryPoints,
#endif
      NaCl_NP_Initialize,
      NaCl_NP_Shutdown
  };

  const NPAPI::PluginVersionInfo nacl_plugin_info = {
      FilePath(FILE_PATH_LITERAL("internal_nacl")),
      L"Native Client",
      L"Statically linked NaCl",
      L"1, 0, 0, 1",
      L"application/x-nacl-srpc",
      L"",
      L"",
      entry_points
  };

  if (NULL != launch_func) {
    launch_nacl_process = launch_func;
  }

  NaClHandlePassBrowserInit();

  NPAPI::PluginList::Singleton()->RegisterInternalPlugin(nacl_plugin_info);
}
