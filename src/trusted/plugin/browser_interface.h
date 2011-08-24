/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// Interface for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "ppapi/cpp/instance.h"

namespace pp {
class InstancePrivate;
}  // namespace

namespace plugin {

// BrowserInterface represents the interface to the browser from the plugin.
// I.e., when the plugin needs to request an alert, it uses these interfaces.
class BrowserInterface {
 public:
  BrowserInterface() : next_identifier(0) {}
  ~BrowserInterface() { }

  // Functions for communication with the browser.

  // Convert a string to an identifier.
  uintptr_t StringToIdentifier(const nacl::string& str);
  // Convert an identifier to a string.
  nacl::string IdentifierToString(uintptr_t ident);

  // Write to the JavaScript console.
  void AddToConsole(pp::InstancePrivate* instance, const nacl::string& text);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserInterface);

  // Map strings used for property and method names to unique ids and back.
  typedef std::map<nacl::string, uintptr_t> StringToIdentifierMap;
  typedef std::map<uintptr_t, nacl::string> IdentifierToStringMap;
  StringToIdentifierMap string_to_identifier_map_;
  IdentifierToStringMap identifier_to_string_map_;
  uintptr_t next_identifier;  // will be incremented once used
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_
