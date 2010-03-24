/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
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

  // TODO(sehr): replace the argn/argv/argc approach with strings and map.
  virtual char** argn() = 0;
  virtual char** argv() = 0;
  virtual int argc() = 0;

  // Functions for communication with the browser.
  // NOTE(gregoryd): some of the functions below may require more information
  // about the browser than is available using PluginIdentifier. In this case
  // these functions can be made virtual and implemented in the
  // browser-specific class, such as SRPC_Plugin
  static uintptr_t GetStrIdentifierCallback(const char *method_name);
  bool Alert(const nacl::string& text);

  bool GetOrigin(nacl::string **origin);

  // To indicate successful loading of a module, invoke the onload handler.
  bool RunOnloadHandler();
  // To indicate unsuccessful loading of a module, invoke the onfail handler.
  bool RunOnfailHandler();
  static void* BrowserAlloc(int size);
  static void BrowserRelease(void* ptr);
  static char *MemAllocStrdup(const char *str);
  // Convert an identifier to a string
  // TODO(gregoryd): this is not thread safe, should return c++ string
  // c.f. http://code.google.com/p/nativeclient/issues/detail?id=376
  static const char* IdentToString(uintptr_t ident);
  bool CheckExecutableVersion(const char *filename);

  bool CheckExecutableVersion(const void* buffer, int32_t size);

  void InitializeIdentifiers();
  virtual ~PortablePluginInterface() {}
 private:
  bool CheckExecutableVersionCommon(const uint8_t *version);

 public:
  static uintptr_t kConnectIdent;
  static uintptr_t kHeightIdent;
  static uintptr_t kInvokeIdent;
  static uintptr_t kLengthIdent;
  static uintptr_t kMapIdent;
  static uintptr_t kOnfailIdent;
  static uintptr_t kOnloadIdent;
  static uintptr_t kModuleReadyIdent;
  static uintptr_t kNaClMultimediaBridgeIdent;
  static uintptr_t kNullNpapiMethodIdent;
  static uintptr_t kPrintIdent;
  static uintptr_t kReadIdent;
  static uintptr_t kSetCommandLogIdent;
  static uintptr_t kShmFactoryIdent;
  static uintptr_t kSignaturesIdent;
  static uintptr_t kSrcIdent;
  static uintptr_t kToStringIdent;
  static uintptr_t kUrlAsNaClDescIdent;
  static uintptr_t kValueOfIdent;
  static uintptr_t kVideoUpdateModeIdent;
  static uintptr_t kWidthIdent;
  static uintptr_t kWriteIdent;

  static uintptr_t kLocationIdent;
  static uintptr_t kHrefIdent;
 private:
  static bool identifiers_initialized;
  static uint8_t const kInvalidAbiVersion;
};

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_
