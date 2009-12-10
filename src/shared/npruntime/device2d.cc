// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface.  2D graphics API.

#include "native_client/src/shared/npruntime/device2d.h"

#include <stdlib.h>
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utility>

#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/npruntime/npnavigator.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace {

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
  static const int kInvalidHandle = -1;
  int desc = kInvalidHandle;
  void* map_addr = MAP_FAILED;
  int stride;

  // NOTE: The config struct is empty, so don't do anything with it.

  // Make the SRPC to request the setup for the context.
  nacl::NPNavigator* nav = nacl::NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      NaClSrpcInvokeByName(channel,
                           "Device2DInitialize",
                           nacl::NPNavigator::GetPluginNPP(instance),
                           &desc,
                           &stride);
  if (NACL_SRPC_RESULT_OK != retval) {
    goto cleanup;
  }
  // Get the shared memory region's size. */
  struct stat st;
  size_t size;

  if (0 != fstat(desc, &st)) {
    goto cleanup;
  }
  size = (size_t) st.st_size;
  // Map the shared memory region.
  map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
  if (MAP_FAILED == map_addr) {
    goto cleanup;
  }
  // Set the context structure.
  // TODO(sehr): remove the the whole u.graphicsRgba name.
  context2d->u.graphicsRgba.region = map_addr;
  context2d->u.graphicsRgba.stride = stride;
  context2d->u.graphicsRgba.dirty.left = 0;
  context2d->u.graphicsRgba.dirty.top = 0;
  context2d->u.graphicsRgba.dirty.right = 0;
  context2d->u.graphicsRgba.dirty.bottom = 0;
  return NPERR_NO_ERROR;

cleanup:
  if (kInvalidHandle != desc) {
    close(desc);
  }
  return NPERR_GENERIC_ERROR;
}

static NPError GetStateContext(NPP instance,
                               NPDeviceContext* context,
                               int32 state,
                               int32 *value) {
  return NPERR_GENERIC_ERROR;
}

static NPError SetStateContext(NPP instance,
                               NPDeviceContext* context,
                               int32 state,
                               int32 value) {
  return NPERR_GENERIC_ERROR;
}

static NPError FlushContext(NPP instance,
                            NPDeviceContext* context,
                            NPDeviceFlushContextCallbackPtr callback,
                            void* userData) {
  // Make the SRPC to request the setup for the context.
  nacl::NPNavigator* nav = nacl::NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      NaClSrpcInvokeByName(channel,
                           "Device2DFlush",
                           nacl::NPNavigator::GetPluginNPP(instance));
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

static NPError DestroyContext(NPP instance,
                              NPDeviceContext* context) {
  // We're leaving the shared memory mapped and the descriptor open here.
  // TODO(sehr): unmap and close the shared memory descriptor.
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
