// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface.  3D graphics API.

#include "native_client/src/shared/npruntime/device3d.h"

#include <inttypes.h>
#include <stdlib.h>
#include <nacl/nacl_inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits>
#include <map>
#include <utility>

#include "native_client/src/include/portability_string.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "native_client/src/shared/npruntime/npnavigator.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::NPNavigator;
using nacl::NPPToWireFormat;

namespace {

static const int kInvalidDesc = -1;

static const intptr_t kInt32Max = std::numeric_limits<int32_t>::max();
static const intptr_t kInt32Min = std::numeric_limits<int32_t>::min();

class Device3DBufferImpl {
 public:
  int desc_;
  size_t size_;
  void* addr_;

  Device3DBufferImpl(int desc, size_t size, void* addr) :
    desc_(desc), size_(size), addr_(addr) { }
  ~Device3DBufferImpl() { }
};

struct Device3DImpl {
  std::map<int32, Device3DBufferImpl*> id_map;
  int shared_memory_desc;
  size_t size;
};

static NPError QueryCapability(NPP instance,
                               int32 capability,
                               int32 *value) {
  return NPERR_GENERIC_ERROR;
}

static NPError QueryConfig(NPP instance,
                           const NPDeviceConfig* request,
                           NPDeviceConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

static NPError InitializeContext(NPP instance,
                                 const NPDeviceConfig* config,
                                 NPDeviceContext* context) {
  const NPDeviceContext3DConfig* config3d =
      reinterpret_cast<const NPDeviceContext3DConfig*>(config);
  NPDeviceContext3D* context3d = reinterpret_cast<NPDeviceContext3D*>(context);
  int shm_desc = kInvalidDesc;
  void* map_addr = MAP_FAILED;
  Device3DImpl* impl = NULL;

  // Initialize the context structure.
  context3d->reserved = NULL;
  context3d->commandBuffer = NULL;
  context3d->commandBufferSize = -1;
  context3d->getOffset = -1;
  context3d->putOffset = -1;

  // Make the SRPC to request the setup for the context.
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      Device3DRpcClient::Device3DInitialize(
          channel,
          NPPToWireFormat(instance),
          config3d->commandBufferSize,
          &shm_desc,
          &context3d->commandBufferSize,
          &context3d->getOffset,
          &context3d->putOffset);
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
  impl = new(std::nothrow) Device3DImpl;
  if (NULL == impl) {
    goto cleanup;
  }
  impl->shared_memory_desc = shm_desc;
  impl->size = size;
  context3d->reserved = impl;
  context3d->commandBuffer = map_addr;
  context3d->error = NPDeviceContext3DError_NoError;
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
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  // Value is only intptr_t to allow passing of the TransportDIB function
  // pointer.  Otherwise they're all int32_t.
  int32_t value32;
  NaClSrpcError retval =
      Device3DRpcClient::Device3DGetState(
          channel,
          NPPToWireFormat(instance),
          state,
          &value32);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  *value = static_cast<intptr_t>(value32);
  return NPERR_NO_ERROR;
}

static NPError SetStateContext(NPP instance,
                               NPDeviceContext* context,
                               int32 state,
                               intptr_t value) {
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  // Value is only intptr_t to allow passing of the TransportDIB function
  // pointer.  Otherwise they're all int32_t.  Verify anyway.
  // TODO(sehr,ilewis): use checked_cast<> here.
  if (kInt32Max < value || kInt32Min > value) {
    return NPERR_GENERIC_ERROR;
  }
  int32_t value32 = static_cast<int32_t>(value);
  NaClSrpcError retval =
      Device3DRpcClient::Device3DSetState(
          channel,
          NPPToWireFormat(instance),
          state,
          value32);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

static NPError FlushContext(NPP instance,
                            NPDeviceContext* context,
                            NPDeviceFlushContextCallbackPtr callback,
                            void* userData) {
  NPDeviceContext3D* context3d = reinterpret_cast<NPDeviceContext3D*>(context);
  NPNavigator* nav = NPNavigator::GetNavigator();
  int32_t error;
  NaClSrpcError retval =
      nav->Device3DFlush(NPPToWireFormat(instance),
                         context3d->putOffset,
                         &context3d->getOffset,
                         &context3d->token,
                         &error);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  context3d->error = static_cast<NPDeviceContext3DError>(error);
  // Invoke the callback.
  // TODO(sehr): the callback seems to be invoked from the wrong place.
  if (NULL != callback) {
    (*callback)(instance, context3d, NPERR_NO_ERROR, userData);
  }
  return NPERR_NO_ERROR;
}

static NPError DestroyContext(NPP instance, NPDeviceContext* context) {
  NPDeviceContext3D* context3d = reinterpret_cast<NPDeviceContext3D*>(context);
  Device3DImpl* impl = NULL;
  // If the context wasn't initialized, return an error.
  if (NULL == context3d->reserved) {
    return NPERR_GENERIC_ERROR;
  }
  impl = reinterpret_cast<Device3DImpl*>(context3d->reserved);
  // Unmap the region.
  if (NULL != context3d->commandBuffer) {
    munmap(context3d->commandBuffer, impl->size);
  }
  // Close the shared memory descriptor.
  if (kInvalidDesc != impl->shared_memory_desc) {
    close(impl->shared_memory_desc);
  }
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      Device3DRpcClient::Device3DDestroy(
          channel,
          NPPToWireFormat(instance));
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

NPError CreateBuffer(NPP instance,
                     NPDeviceContext* context,
                     size_t size,
                     int32* id) {
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NPDeviceContext3D* context3d = reinterpret_cast<NPDeviceContext3D*>(context);
  Device3DImpl* impl = reinterpret_cast<Device3DImpl*>(context3d->reserved);
  int shm_desc = kInvalidDesc;
  void* map_addr = MAP_FAILED;
  Device3DBufferImpl* buffer_impl = NULL;
  NaClSrpcError retval = NACL_SRPC_RESULT_APP_ERROR;

  *id = -1;
  if (NULL == impl) {
    goto cleanup;
  }
  retval =
      Device3DRpcClient::Device3DCreateBuffer(
          channel,
          NPPToWireFormat(instance),
          static_cast<int32_t>(size),
          &shm_desc,
          id);
  if (NACL_SRPC_RESULT_OK != retval) {
    goto cleanup;
  }
  buffer_impl =
      new(std::nothrow) Device3DBufferImpl(shm_desc, size, MAP_FAILED);
  if (NULL == buffer_impl) {
    goto cleanup;
  }
  impl->id_map[*id] = buffer_impl;
  return NPERR_NO_ERROR;

 cleanup:
  if (NULL != buffer_impl) {
    delete buffer_impl;
  }
  if (MAP_FAILED != map_addr) {
    munmap(map_addr, size);
  }
  if (kInvalidDesc != shm_desc) {
    close(shm_desc);
  }
  return NPERR_GENERIC_ERROR;
}

NPError DestroyBuffer(NPP instance,
                      NPDeviceContext* context,
                      int32 id) {
  NPDeviceContext3D* context3d = reinterpret_cast<NPDeviceContext3D*>(context);
  Device3DImpl* impl = reinterpret_cast<Device3DImpl*>(context3d->reserved);
  std::map<int32, Device3DBufferImpl*>::iterator iter;
  Device3DBufferImpl* buffer_impl = NULL;

  if (NULL == impl) {
    goto error;
  }
  iter = impl->id_map.find(id);
  if (impl->id_map.end() == iter) {
    goto error;
  }
  buffer_impl = iter->second;
  if (NULL == buffer_impl) {
    goto error;
  }
  if (MAP_FAILED != buffer_impl->addr_) {
    munmap(buffer_impl->addr_, buffer_impl->size_);
  }
  if (kInvalidDesc != buffer_impl->desc_) {
    close(buffer_impl->desc_);
  }
  impl->id_map.erase(id);
  return NPERR_NO_ERROR;

 error:
  return NPERR_GENERIC_ERROR;
}

NPError MapBuffer(NPP instance,
                  NPDeviceContext* context,
                  int32 id,
                  NPDeviceBuffer* buffer) {
  NPDeviceContext3D* context3d = reinterpret_cast<NPDeviceContext3D*>(context);
  Device3DImpl* impl = reinterpret_cast<Device3DImpl*>(context3d->reserved);
  std::map<int32, Device3DBufferImpl*>::iterator iter;
  Device3DBufferImpl* buffer_impl = NULL;
  void* map_addr = MAP_FAILED;

  if (NULL == buffer) {
    goto error;
  }
  if (NULL == impl) {
    goto error;
  }
  iter = impl->id_map.find(id);
  if (impl->id_map.end() == iter) {
    goto error;
  }
  buffer_impl = iter->second;
  if (NULL == buffer_impl) {
    goto error;
  }
  if (MAP_FAILED != buffer_impl->addr_) {
    // Already mapped.
    buffer->ptr = buffer_impl->addr_;
    buffer->size = buffer_impl->size_;
  }
  // Map the descriptor in
  map_addr = mmap(0,
                  buffer_impl->size_,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  buffer_impl->desc_,
                  0);
  if (MAP_FAILED == map_addr) {
    goto error;
  }
  // Save the results in the mapping.
  buffer_impl->addr_ = map_addr;
  // Return the results.
  buffer->ptr = buffer_impl->addr_;
  buffer->size = buffer_impl->size_;
  return NPERR_NO_ERROR;

 error:
  return NPERR_GENERIC_ERROR;
}

}  // namespace

namespace nacl {

const NPDevice* GetDevice3D() {
  static const struct NPDevice device3D = {
    QueryCapability,
    QueryConfig,
    InitializeContext,
    SetStateContext,
    GetStateContext,
    FlushContext,
    DestroyContext,
    CreateBuffer,
    DestroyBuffer,
    MapBuffer
  };

  return &device3D;
}

}  // namespace nacl
