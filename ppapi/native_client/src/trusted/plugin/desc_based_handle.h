/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// OBSOLETE -- this class provides support for passing NaClDescs between
// plugin instances.  This support only exists in SRPC-mode plugins.
// TODO(polina): remove when SRPC descriptor passing is removed.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DESC_BASED_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DESC_BASED_HANDLE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/utility.h"

struct NaClDesc;

namespace nacl {
class DescWrapper;
}  // namespace nacl

namespace plugin {

class Plugin;

// DescBasedHandles are used to convey NaClDesc objects through JavaScript.
class DescBasedHandle {
 public:
  // Creates a new DescBasedHandle using the specified plugin and wrapper.
  // Returns NULL if either plugin or wrapper is NULL.
  static DescBasedHandle* New(nacl::DescWrapper* wrapper);
  ~DescBasedHandle();

  // Because the factory ensured that wrapper was not NULL, dereferencing it
  // here is always safe.
  NaClDesc* desc() const { return wrapper_->desc(); }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DescBasedHandle);
  explicit DescBasedHandle(nacl::DescWrapper* wrapper);
  nacl::scoped_ptr<nacl::DescWrapper> wrapper_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DESC_BASED_HANDLE_H_
