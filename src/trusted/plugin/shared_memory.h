/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A portable representation of a scriptable shared memory object.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SHARED_MEMORY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SHARED_MEMORY_H_

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

class Plugin;

// SharedMemory is used to represent shared memory descriptors.
class SharedMemory : public DescBasedHandle {
 public:
  static SharedMemory* New(Plugin* plugin, nacl::DescWrapper* wrapper);
  static SharedMemory* New(Plugin* plugin, off_t length);

  virtual void* shm_addr() const { return addr_; }
  virtual size_t shm_size() const { return size_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SharedMemory);
  // These are private because there are no derived classes from
  // SharedMemory.
  SharedMemory();
  ~SharedMemory();
  bool Init(Plugin* plugin, nacl::DescWrapper* wrapper, off_t length);
  void LoadMethods();
  NaClHandle handle_;
  void* addr_;
  size_t size_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SHARED_MEMORY_H_
