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


// Portable interface for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_

#include "native_client/src/include/portability.h"

#include <stdio.h>
#include <map>
#include <string>
#include "native_client/src/include/base/basictypes.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl_srpc {
  typedef NPP PluginIdentifier;

  template <typename HandleType>
  class ScriptableHandle;
  class Plugin;
}

namespace nacl {
  class VideoMap;
  class NPModule;
}

// PortablePluginInterface represents the interface to the browser from
// the plugin, independent of whether it is the ActiveX or NPAPI instance.
// I.e., when the plugin needs to request an alert, it uses these interfaces.
class PortablePluginInterface {
 public:
  virtual nacl_srpc::PluginIdentifier GetPluginIdentifier() = 0;
  virtual nacl::VideoMap* video() = 0;
  virtual nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* plugin() = 0;
  virtual nacl::NPModule* module() = 0;
  virtual void set_module(nacl::NPModule* module)  = 0;
  virtual NPObject* nacl_instance() = 0;

  // Functions for communication with the browser.
  // NOTE(gregoryd): some of the functions below may require more information
  // about the browser than is available using PluginIdentifier. In this case
  // these functions can be made virtual and implemented in the
  // browser-specific class, such as SRPC_Plugin
  static uintptr_t GetStrIdentifierCallback(const char *method_name);
  static bool Alert(nacl_srpc::PluginIdentifier plugin_identifier,
                    const char *text,
                    int length);
  static bool GetOrigin(nacl_srpc::PluginIdentifier plugin_identifier,
                        std::string **origin);
  static void* BrowserAlloc(int size);
  static void BrowserRelease(void* ptr);
  static char *MemAllocStrdup(const char *str);
  // Convert an identifier to a string
  static const char* IdentToString(uintptr_t ident);
  static bool CheckExecutableVersion(nacl_srpc::PluginIdentifier instance,
                                     const char *filename);

  static bool CheckExecutableVersion(nacl_srpc::PluginIdentifier instance,
                                     const void* buffer,
                                     int32_t size);
  void InitializeIdentifiers();
  virtual ~PortablePluginInterface() {}
 private:
  static bool CheckExecutableVersionCommon(nacl_srpc::PluginIdentifier instance,
                                           const char *version);
 public:
  static int kConnectIdent;
  static int kHeightIdent;
  static int kInvokeIdent;
  static int kLengthIdent;
  static int kMapIdent;
  static int kOnfailIdent;
  static int kOnloadIdent;
  static int kModuleReadyIdent;
  static int kNaClMultimediaBridgeIdent;
  static int kNullNpapiMethodIdent;
  static int kPrintIdent;
  static int kReadIdent;
  static int kSetCommandLogIdent;
  static int kShmFactoryIdent;
  static int kSignaturesIdent;
  static int kSrcIdent;
  static int kToStringIdent;
  static int kUrlAsNaClDescIdent;
  static int kValueOfIdent;
  static int kVideoUpdateModeIdent;
  static int kWidthIdent;
  static int kWriteIdent;

  static int kLocationIdent;
  static int kHrefIdent;
 private:
  static bool identifiers_initialized;
  static uint8_t const kInvalidAbiVersion;
};

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_
