// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface.  Audio API.
// This file implements the untrusted client interface to audio.

#include "native_client/src/shared/npruntime/audio.h"

#include <inttypes.h>
#include <stdlib.h>
#include <nacl/nacl_inttypes.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>
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
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::NPClosureTable;
using nacl::NPNavigator;
using nacl::NPPToWireFormat;
using nacl::DebugPrintf;

namespace {

static const int kInvalidDesc = -1;

struct AudioImpl {
 public:
  NPDeviceContextAudio* context;
  int shared_memory_desc;
  size_t shared_memory_size;
  int sync_desc;
};

static NPError QueryCapability(NPP instance,
                               int32 capability,
                               int32 *value) {
  // UNREFERENCED_PARAMETER(instance);
  // UNREFERENCED_PARAMETER(capability);
  // UNREFERENCED_PARAMETER(value);
  // There is no corresponding function in the trusted pepper implementation.
  return NPERR_GENERIC_ERROR;
}

static NPError QueryConfig(NPP instance,
                           const NPDeviceConfig* request,
                           NPDeviceConfig* obtain) {
  // UNREFERENCED_PARAMETER(instance);
  // UNREFERENCED_PARAMETER(request);
  // UNREFERENCED_PARAMETER(obtain);
  // There is no corresponding function in the trusted pepper implementation.
  return NPERR_GENERIC_ERROR;
}

static NPError InitializeContext(NPP instance,
                                 const NPDeviceConfig* config,
                                 NPDeviceContext* context) {
  DebugPrintf("Audio::InitializeContext %p %p\n",
              reinterpret_cast<const void*>(config),
              reinterpret_cast<void*>(context));
  const NPDeviceContextAudioConfig* config_audio =
      reinterpret_cast<const NPDeviceContextAudioConfig*>(config);
  NPDeviceContextAudio* context_audio =
      reinterpret_cast<NPDeviceContextAudio*>(context);

  // Initialize the context structure.
  context_audio->config = *config_audio;
  context_audio->outBuffer = NULL;
  context_audio->inBuffer = NULL;
  context_audio->reserved = NULL;

  DebugPrintf("Set the config and base stuff\n");
  // Get the configuration parameters for passing.
  // TODO(sehr): the SRPC generators speak int32_t, while npruntime likes
  // int32.  Find a way to avoid all these casts.
  int32_t sample_rate = static_cast<int32_t>(config_audio->sampleRate);
  int32_t sample_type = static_cast<int32_t>(config_audio->sampleType);
  int32_t output_channel_map =
      static_cast<int32_t>(config_audio->outputChannelMap);
  int32_t input_channel_map =
      static_cast<int32_t>(config_audio->inputChannelMap);
  int32_t sample_frame_count =
      static_cast<int32_t>(config_audio->sampleFrameCount);
  int32_t flags = static_cast<int32_t>(config_audio->flags);
  DebugPrintf("Set the configs\n");

  // Create an implementation structure to hold the descriptors
  // when they are returned.
  AudioImpl* impl = new(std::nothrow) AudioImpl();
  if (NULL == impl) {
    return NPERR_GENERIC_ERROR;
  }
  DebugPrintf("Created the impl\n");
  context_audio->reserved = reinterpret_cast<void*>(impl);
  // Remember the context this is for so that the callback can get it.
  impl->context = context_audio;
  // Create a closure to be called when NPModule calls us back with
  // the descriptors.  We are remembering the user-supplied callback
  // function and our implementation data.  When the RPC to invoke the
  // callback comes from the browser, we will copy some descriptors into
  // the implementation and then invoke the user-supplied callback with
  // the context.
  uint32_t id;
  NPNavigator* nav = NPNavigator::GetNavigator();
  NPClosureTable::FunctionPointer func =
      reinterpret_cast<NPClosureTable::FunctionPointer>(
          context_audio->config.callback);
  // Pedantic mode refuses zu format, hence casting to void*. The cast to size_t
  // fixes the GCC warning about the cast of function ptr to data ptr.
  DebugPrintf("Created the closure %p for %p\n",
              reinterpret_cast<void*>(reinterpret_cast<size_t>(func)),
              reinterpret_cast<void*>(
                  reinterpret_cast<size_t>(context_audio->config.callback)));
  if (NULL == nav->closure_table() ||
      !nav->closure_table()->Add(func,
                                 context_audio->reserved,
                                 &id)) {
    return NPERR_GENERIC_ERROR;
  }
  DebugPrintf("Added the closure %p\n",
              reinterpret_cast<void*>(reinterpret_cast<size_t>(func)));
  // Make the SRPC to request the setup for the context.
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      AudioRpcClient::AudioInitialize(
          channel,
          NPPToWireFormat(instance),
          id,
          sample_rate,
          sample_type,
          output_channel_map,
          input_channel_map,
          sample_frame_count,
          flags);
  DebugPrintf("Called init\n");
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

static NPError GetStateContext(NPP instance,
                               NPDeviceContext* context,
                               int32 state,
                               intptr_t *value) {
  // UNREFERENCED_PARAMETER(context);
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  // Value is only intptr_t to allow passing of the TransportDIB function
  // pointer.  Otherwise they're all int32_t.
  int32_t value32;
  NaClSrpcError retval =
      AudioRpcClient::AudioGetState(
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
  // UNREFERENCED_PARAMETER(context);
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  // Value is only intptr_t to allow passing of the TransportDIB function
  // pointer.  Otherwise they're all int32_t.  Verify anyway.
  // TODO(sehr,ilewis): use checked_cast<> here.
  if (INT32_MAX < value || INT32_MIN > value) {
    return NPERR_GENERIC_ERROR;
  }
  int32_t value32 = static_cast<int32_t>(value);
  NaClSrpcError retval =
      AudioRpcClient::AudioSetState(
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
  // UNREFERENCED_PARAMETER(instance);
  // UNREFERENCED_PARAMETER(context);
  // UNREFERENCED_PARAMETER(callback);
  // UNREFERENCED_PARAMETER(userData);
  // NOTE: this method does not appear to be used in the current version of the
  // Pepper API.  Instead, the callback from InitializeContext invokes a method
  // that will synchronize by means of impl->sync_desc.
  return NPERR_GENERIC_ERROR;
}

static NPError DestroyContext(NPP instance, NPDeviceContext* context) {
  NPDeviceContextAudio* context_audio =
      reinterpret_cast<NPDeviceContextAudio*>(context);
  AudioImpl* impl = NULL;
  // If the context wasn't initialized, return an error.
  if (NULL == context_audio->reserved) {
    return NPERR_GENERIC_ERROR;
  }
  impl = reinterpret_cast<AudioImpl*>(context_audio->reserved);
  if (NULL != context_audio->outBuffer) {
    munmap(context_audio->outBuffer, impl->shared_memory_size);
    context_audio->outBuffer = NULL;
  }
  // Close the shared memory descriptor.
  if (kInvalidDesc != impl->shared_memory_desc) {
    close(impl->shared_memory_desc);
  }
  // Close the sync descriptor.
  if (kInvalidDesc != impl->sync_desc) {
    close(impl->sync_desc);
  }
  // Remove the implementation artifacts.
  delete impl;
  context_audio->reserved = NULL;
  // And destroy the corresponding structure in the renderer.
  NPNavigator* nav = NPNavigator::GetNavigator();
  NaClSrpcChannel* channel = nav->channel();
  NaClSrpcError retval =
      AudioRpcClient::AudioDestroy(
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
  // There is no corresponding function in the trusted pepper implementation.
  return NPERR_GENERIC_ERROR;
}

NPError DestroyBuffer(NPP instance,
                      NPDeviceContext* context,
                      int32 id) {
  // There is no corresponding function in the trusted pepper implementation.
  return NPERR_GENERIC_ERROR;
}

NPError MapBuffer(NPP instance,
                  NPDeviceContext* context,
                  int32 id,
                  NPDeviceBuffer* buffer) {
  // There is no corresponding function in the trusted pepper implementation.
  return NPERR_GENERIC_ERROR;
}

// Some clients may use --wrap read, which replaces read by a user-specified
// alternate function.  That will cause major problems here as we need to use
// the real read syscall.  To handle this, we add a static binding to the
// real system call.
// TODO(sehr): All of this support will be unnecessary once we have file
// system access from Pepper.  Remove this once we do.
static ssize_t socketread(int desc, void *buf, size_t count) {
  ssize_t retval = NACL_SYSCALL(read)(desc, buf, count);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}

static void* AudioCallbackThread(void* data) {
  NPDeviceContextAudio* context_audio =
      reinterpret_cast<NPDeviceContextAudio*>(data);
  DebugPrintf("AudioCallbackThread %p\n", data);
  if (NULL == context_audio) {
    return NULL;
  }
  DebugPrintf("AudioCallbackThread impl %p\n", context_audio->reserved);
  AudioImpl* impl = reinterpret_cast<AudioImpl*>(context_audio->reserved);
  if (NULL == impl) {
    return NULL;
  }
  DebugPrintf("AudioCallbackThread entering loop\n");
  int pending_data;
  while (sizeof(pending_data) ==
         socketread(impl->sync_desc, &pending_data, sizeof(pending_data))) {
    DebugPrintf("invoking callback\n");
    if (NULL != context_audio->config.callback) {
      context_audio->config.callback(context_audio);
    }
  }
  return NULL;
}

}  // namespace

namespace nacl {

const NPDevice* GetAudio() {
  static const struct NPDevice deviceAudio = {
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

  return &deviceAudio;
}

bool DoAudioCallback(NPClosureTable* closure_table,
                     int32_t number,
                     int shm_desc,
                     int32_t shm_size,
                     int sync_desc) {
  uint32_t id = static_cast<uint32_t>(number);
  NPClosureTable::FunctionPointer func;
  void* user_data;
  DebugPrintf("DoAudioCallback\n");
  if (NULL == closure_table ||
      !closure_table->Remove(id, &func, &user_data) ||
      NULL == func ||
      NULL == user_data) {
    return false;
  }
  // Place the returned handle in the struct passed as user_data.
  AudioImpl* impl = reinterpret_cast<AudioImpl*>(user_data);
  impl->sync_desc = sync_desc;
  impl->shared_memory_desc = shm_desc;
  impl->shared_memory_size = shm_size;
  void* buf = mmap(NULL,
                   shm_size,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   shm_desc,
                   0);
  if (MAP_FAILED == buf) {
    DebugPrintf("mmap failed %"NACL_PRIx32" %d %d\n",
                shm_size, shm_desc, sync_desc);
    return false;
  }
  DebugPrintf("mmap succeeded %p\n", buf);
  impl->context->outBuffer = buf;
  if (impl->context->config.startThread) {
    DebugPrintf("starting thread\n");
    pthread_t callback_thread;
    int ret = pthread_create(&callback_thread,
                             NULL,
                             AudioCallbackThread,
                             impl->context);
    if (0 != ret) {
      DebugPrintf("Thread creation failed %d %d\n", ret, errno);
      return false;
    }
  } else {
    // Invoke the function with the data.
    (*func)(reinterpret_cast<void*>(impl->context));
  }
  // Return success.
  return true;
}

}  // namespace nacl
