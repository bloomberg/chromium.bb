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

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_

#include <string.h>
#include <set>

#include "native_client/src/shared/npruntime/npbridge.h"

namespace nacl {

// Represents the NaCl module end of the connection to the browser plugin. The
// opposite end is the NPModule.
class NPNavigator : public NPBridge {
  // Compares two strings in string_set_.
  struct StringCompare {
    bool operator()(const NPUTF8* left, const NPUTF8* right) const {
      return (strcmp(left, right) < 0) ? true : false;
    }
  };

  // The number of argument passed from the plugin.
  int* argc_;
  // The array of arguments passed from the plugin.
  char** argv_;

  // The set of character strings for NPIdentifier names.
  std::set<const NPUTF8*, StringCompare> string_set_;

  // The following three member variables save the user parameters passed to
  // OpenURL().
  //
  // The callback function to be called upon a successful OpenURL() invocation.
  void (*notify_)(const char* url, void* notify_data, HtpHandle handle);
  // The user supplied pointer to the user data.
  void* notify_data_;
  // The URL to open.
  char* url_;

  // The NPWindow for this NaCl module.
  NPWindow window_;
  // The shared memory object handle received from the plugin into which the
  // window content is rendered.
  HtpHandle bitmap_shm_;

 public:
  // Creates a new instance of NPNavigator. argc and argv should be unmodified
  // variables from NaClNP_Init(). npp must be a pointer to an NPP_t structure
  // to be used for the NaCl module.
  NPNavigator(NPP npp, int* argc, char* argv[]);
  ~NPNavigator();

  // Dispatches the received request message.
  virtual int Dispatch(RpcHeader* request, int length);

  // Processes NPP_New() request from the plugin.
  int New(RpcHeader* request, int length);
  // Processes NPP_Destroy() request from the plugin.
  int Destroy(RpcHeader* request, int length);
  // Processes NPP_GetScriptableInstance() request from the plugin.
  int GetPluginScriptableObject(RpcHeader* request, int length);
  // Processes NPP_URLNotify() request from the plugin.
  int URLNotify(RpcHeader* request, int length);

  // Sends NPN_Status() request to the plugin.
  void SetStatus(const char* message);
  // Sends NPN_GetValue() request to the plugin.
  NPError GetValue(nacl::RpcType, void* value);
  // Sends NaClNPN_CreateArray() request to the plugin.
  NPObject* CreateArray();
  // Sends NaClNPN_OpenURL() request to the plugin.
  NPError OpenURL(const char* url, void* notify_data,
                  void (*notify)(const char* url, void* notify_data,
                                 HtpHandle handle));
  // Sends NPN_InvalidateRect() request to the plugin.
  void InvalidateRect(NPRect* invalid_rect);
  // Sends NPN_ForceRedraw() request to the plugin.
  void ForceRedraw();

  // Gets the character string from string_set_ that matches the specified name.
  const NPUTF8* GetStringIdentifier(const NPUTF8* name);

  // Returns true if name is a string NPIdentifier.
  // Note NPN_IdentifierIsString() inside the browser cannot be used to detect
  // string identifiers being transferred from the NaCl module over the IMC
  // channel.
  static bool IdentifierIsString(NPIdentifier name) {
    return (reinterpret_cast<intptr_t>(name) & 1) ? false : true;
  }
};

}  // namespace nacl

nacl::NPNavigator* NaClNP_GetNavigator();

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_
