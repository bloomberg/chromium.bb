/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI Simple RPC shared memory objects.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SHARED_MEMORY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SHARED_MEMORY_H_

#include <setjmp.h>
#include <stdio.h>
#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

// Class declarations.
class Plugin;

struct SharedMemoryInitializer: DescHandleInitializer {
 public:
  SharedMemoryInitializer(BrowserInterface* browser_interface,
                          nacl::DescWrapper* desc,
                          Plugin* plugin):
      DescHandleInitializer(browser_interface, desc, plugin),
      length_(0) {}

  SharedMemoryInitializer(BrowserInterface* browser_interface,
                          Plugin* plugin,
                          off_t length):
      DescHandleInitializer(browser_interface, NULL, plugin),
      length_(length) {}

 public:
  off_t length_;
};

// SharedMemory is used to represent shared memory descriptors
class SharedMemory : public DescBasedHandle {
 public:
  explicit SharedMemory();
  ~SharedMemory();

  bool Init(struct PortableHandleInitializer* init_info);

  void LoadMethods();
  static int number_alive() { return number_alive_counter; }
  void* buffer() { return map_addr_; }


 private:
  static void SignalHandler(int value);

  // registered functions
  static bool RpcRead(void* obj, SrpcParams* params);
  static bool RpcWrite(void* obj, SrpcParams* params);

 private:
  static int number_alive_counter;
  NaClHandle handle_;
  void* map_addr_;
  size_t size_;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SHARED_MEMORY_H_
