/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Portable representation of scriptable NaClDesc.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_DESC_BASED_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_DESC_BASED_HANDLE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

struct NaClDesc;

namespace nacl {
class DescWrapper;
}  // namespace nacl

namespace plugin {

class Plugin;

// DescBasedHandles are used to make NaClDesc objects scriptable by JavaScript.
class DescBasedHandle : public PortableHandle {
 public:
  static DescBasedHandle* New(Plugin* plugin, nacl::DescWrapper* wrapper);

  virtual BrowserInterface* browser_interface() const;
  virtual Plugin* plugin() const { return plugin_; }
  // Get the contained descriptor.
  virtual nacl::DescWrapper* wrapper() const { return wrapper_; }
  virtual NaClDesc* desc() const {
    if (NULL == wrapper_) {
      return NULL;
    }
    return wrapper_->desc();
  }

 protected:
  DescBasedHandle();
  virtual ~DescBasedHandle();
  bool Init(Plugin* plugin, nacl::DescWrapper* wrapper);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DescBasedHandle);
  void LoadMethods();
  Plugin* plugin_;
  nacl::DescWrapper* wrapper_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_DESC_BASED_HANDLE_H_
