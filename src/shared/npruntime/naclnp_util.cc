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


// NaCl-NPAPI Interface
// This directory is shared because it implements the ability to create either
// a trusted or untrusted plugin communicating with the NPAPI plugin interface.

#include <stdio.h>

#include <new>

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npnavigator.h"

#ifndef __native_client__
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#endif

namespace {

// Holds the global value of the plugin instance tracking data (NPP).
// This value is used to tell the browser which plugin instance is
// talking to it.
// This is not thread safe.
NPP_t g_npp;

}  // namespace

int NaClNP_Init(int* argc, char* argv[]) {
#ifndef __native_client__
  NaClNrdAllModulesInit();
#endif
  g_npp.pdata = NULL;
  g_npp.ndata = new(std::nothrow) nacl::NPNavigator(&g_npp, argc, argv);
  return 0;
}

int NaClNP_MainLoop(unsigned flags) {
  if (g_npp.ndata == NULL) {
    return -1;
  }
  nacl::NPNavigator* navigator = NaClNP_GetNavigator();
  navigator->Wait(NULL, NULL);
  delete navigator;
#ifndef __native_client__
  NaClNrdAllModulesFini();
#endif
  return 0;
}

NPObject* NaClNPN_CreateArray(NPP npp) {
  nacl::NPNavigator* navigator = static_cast<nacl::NPNavigator*>(npp->ndata);
  if (navigator == NULL) {
    return NULL;
  }
  return navigator->CreateArray();
}

NPError NaClNPN_OpenURL(NPP npp, const char* url, void* notify_data,
                        void (*notify)(const char* url, void* notify_data,
                                       nacl::HtpHandle handle)) {
  nacl::NPNavigator* navigator = static_cast<nacl::NPNavigator*>(npp->ndata);
  if (navigator == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  return navigator->OpenURL(url, notify_data, notify);
}

nacl::NPNavigator* NaClNP_GetNavigator() {
  return static_cast<nacl::NPNavigator*>(g_npp.ndata);
}
