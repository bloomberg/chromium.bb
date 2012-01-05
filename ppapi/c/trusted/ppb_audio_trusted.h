/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_audio_trusted.idl modified Wed Oct  5 14:06:02 2011. */

#ifndef PPAPI_C_TRUSTED_PPB_AUDIO_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_AUDIO_TRUSTED_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_AUDIO_TRUSTED_INTERFACE_0_6 "PPB_AudioTrusted;0.6"
#define PPB_AUDIO_TRUSTED_INTERFACE PPB_AUDIO_TRUSTED_INTERFACE_0_6

/**
 * @file
 * This file defines the trusted audio interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * This interface is to be used by proxy implementations.  All
 * functions should be called from the main thread only.  The
 * resource returned is an Audio resource; most of the PPB_Audio
 * interface is also usable on this resource.
 */
struct PPB_AudioTrusted_0_6 {
  /** Returns an audio resource. */
  PP_Resource (*CreateTrusted)(PP_Instance instance);
  /**
   * Opens a paused audio interface, used by trusted side of proxy.
   * Returns PP_ERROR_WOULD_BLOCK on success, and invokes
   * the |create_callback| asynchronously to complete.
   * As this function should always be invoked from the main thread,
   * do not use the blocking variant of PP_CompletionCallback.
   */
  int32_t (*Open)(PP_Resource audio,
                  PP_Resource config,
                  struct PP_CompletionCallback create_callback);
  /**
   * Get the sync socket.  Use once Open has completed.
   * Returns PP_OK on success.
   */
  int32_t (*GetSyncSocket)(PP_Resource audio, int* sync_socket);
  /**
   * Get the shared memory interface.  Use once Open has completed.
   * Returns PP_OK on success.
   */
  int32_t (*GetSharedMemory)(PP_Resource audio,
                             int* shm_handle,
                             uint32_t* shm_size);
};

typedef struct PPB_AudioTrusted_0_6 PPB_AudioTrusted;
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_AUDIO_TRUSTED_H_ */

