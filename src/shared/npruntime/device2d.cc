// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface.  2D graphics API.

#include "native_client/src/shared/npruntime/device2d.h"

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utility>

#include "native_client/src/include/portability_string.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/npruntime/npnavigator.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::NPNavigator;
using nacl::NPPToWireFormat;

namespace {

static const int kInvalidDesc = -1;

struct Device2DImpl {
  int shared_memory_desc;
  size_t size;
};

static NPError QueryCapability(NPP instance,
                               int32 capability,
                               int32 *value) {
  // The config struct is empty, so return error.
  return NPERR_GENERIC_ERROR;
}

static NPError QueryConfig(NPP instance,
                           const NPDeviceConfig* request,
                           NPDeviceConfig* obtain) {
  // The config struct is empty, so return error.
  return NPERR_GENERIC_ERROR;
}

static NPError InitializeContext(NPP instance,
                                 const NPDeviceConfig* config,
                                 NPDeviceContext* context) {
  NPDeviceContext2D* context2d = reinterpret_cast<NPDeviceContext2D*>(context);
  int shm_desc = kInvalidDesc;
  void* map_addr = MAP_FAILED;
  Device2DImpl* impl = NULL;

  // Initialize the context structure.
  context2d->reserved = NULL;
  context2d->region = NULL;
  // NOTE: The config struct is empty, so don't do anything with it.

  // Make the SRPC to request the setup for the context.
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      Device2DRpcClient::Device2DInitialize(
          channel,
          NPPToWireFormat(instance),
          &shm_desc,
          &context2d->stride,
          &context2d->dirty.left,
          &context2d->dirty.top,
          &context2d->dirty.right,
          &context2d->dirty.bottom);
  if (NACL_SRPC_RESULT_OK != retval) {
    goto cleanup;
  }
  // Get the shared memory region's size. */
  struct stat st;
  size_t size;

  if (0 != fstat(shm_desc, &st)) {
    goto cleanup;
  }
  size = static_cast<size_t>(st.st_size);
  // Map the shared memory region.
  map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_desc, 0);
  if (MAP_FAILED == map_addr) {
    goto cleanup;
  }
  // Set the context structure.
  impl = new(std::nothrow) Device2DImpl;
  if (NULL == impl) {
    goto cleanup;
  }
  impl->shared_memory_desc = shm_desc;
  impl->size = size;
  context2d->reserved = impl;
  context2d->region = map_addr;
  return NPERR_NO_ERROR;

 cleanup:
  if (MAP_FAILED != map_addr) {
    munmap(map_addr, size);
  }
  if (kInvalidDesc != shm_desc) {
    close(shm_desc);
  }
  delete impl;
  return NPERR_GENERIC_ERROR;
}

static NPError GetStateContext(NPP instance,
                               NPDeviceContext* context,
                               int32 state,
                               intptr_t *value) {
  // TODO(sehr): Add an RPC for this when 2D trusted API implements it.
  return NPERR_GENERIC_ERROR;
}

static NPError SetStateContext(NPP instance,
                               NPDeviceContext* context,
                               int32 state,
                               intptr_t value) {
  // TODO(sehr): Add an RPC for this when 2D trusted API implements it.
  return NPERR_GENERIC_ERROR;
}

static NPError FlushContext(NPP instance,
                            NPDeviceContext* context,
                            NPDeviceFlushContextCallbackPtr callback,
                            void* userData) {
  NPDeviceContext2D* context2d = reinterpret_cast<NPDeviceContext2D*>(context);
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcError retval =
      Device2DRpcClient::Device2DFlush(nav->channel(),
                                       NPPToWireFormat(instance),
                                       &context2d->stride,
                                       &context2d->dirty.left,
                                       &context2d->dirty.top,
                                       &context2d->dirty.right,
                                       &context2d->dirty.bottom);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  // Invoke the callback.
  // TODO(sehr): the callback seems to be invoked from the wrong place.
  if (NULL != callback) {
    (*callback)(instance, context2d, NPERR_NO_ERROR, userData);
  }
  return NPERR_NO_ERROR;
}

static NPError DestroyContext(NPP instance, NPDeviceContext* context) {
  NPDeviceContext2D* context2d = reinterpret_cast<NPDeviceContext2D*>(context);
  Device2DImpl* impl = NULL;
  // If the context wasn't initialized, return an error.
  if (NULL == context2d->reserved) {
    return NPERR_GENERIC_ERROR;
  }
  impl = reinterpret_cast<Device2DImpl*>(context2d->reserved);
  // Unmap the region.
  if (NULL != context2d->region) {
    munmap(context2d->region, impl->size);
  }
  // Close the shared memory descriptor.
  if (kInvalidDesc != impl->shared_memory_desc) {
    close(impl->shared_memory_desc);
  }
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      Device2DRpcClient::Device2DDestroy(
          channel,
          NPPToWireFormat(instance));
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}


}  // namespace

namespace nacl {

const NPDevice* GetDevice2D() {
  static const struct NPDevice device2D = {
    QueryCapability,
    QueryConfig,
    InitializeContext,
    SetStateContext,
    GetStateContext,
    FlushContext,
    DestroyContext
  };

  return &device2D;
}

}  // namespace nacl
