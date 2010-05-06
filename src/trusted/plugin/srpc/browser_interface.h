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

  // Returns the argument value for the specified key, or NULL if not found.
  // The callee retains ownership of the result.
  virtual char* LookupArgument(const char* key);

  // Functions for communication with the browser.
  // NOTE(gregoryd): some of the functions below may require more information
  // about the browser than is available using PluginIdentifier. In this case
  // these functions can be made virtual and implemented in the
  // browser-specific class, such as SRPC_Plugin
  static uintptr_t GetStrIdentifierCallback(const char *method_name);
  // Pops up an alert box.
  // Returns false if that failed for any reason.  (Is that useful?)
  bool Alert(const nacl::string& text);

  bool GetOrigin(nacl::string **origin);

  // To indicate successful loading of a module, invoke the onload handler.
  bool RunOnloadHandler();
  // To indicate unsuccessful loading of a module, invoke the onfail handler.
  bool RunOnfailHandler();

  // Returns a buffer of length |size|, allocated by the browser.
  // Must be subsequently freed by BrowserRelease, not free().
  // Precondition: size < 2^32.
  static void* BrowserAlloc(size_t size);
  // Releases a buffer previously allocated by BrowserAlloc().  Do not
  // mix with malloc().
  static void BrowserRelease(void* ptr);
  // TODO(adonovan): rename.  The implementation is just strdup.
  static char *MemAllocStrdup(const char *str);
  // Convert an identifier to a string
  static nacl::string IdentToString(uintptr_t ident);
  bool CheckExecutableVersion(const char *filename);

  // Returns true iff |filename| appears to be a valid ELF file;
  // calls Alert() with an informative message otherwise.
  bool CheckElfExecutable(const char *filename);

  // Returns true iff the first |size| bytes of |buffer| appear to be
  // a valid ELF file; calls Alert() with an informative message
  // otherwise.
  bool CheckElfExecutable(const char* buffer, size_t size);

  void InitializeIdentifiers();
  virtual ~PortablePluginInterface() {}

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
