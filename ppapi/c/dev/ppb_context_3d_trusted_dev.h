// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_CONTEXT_3D_TRUSTED_DEV_H_
#define PPAPI_C_DEV_PPB_CONTEXT_3D_TRUSTED_DEV_H_

#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_CONTEXT_3D_TRUSTED_DEV_INTERFACE "PPB_Context3DTrusted(Dev);0.2"

typedef enum {
  kNoError,
  kInvalidSize,
  kOutOfBounds,
  kUnknownCommand,
  kInvalidArguments,
  kLostContext,
  kGenericError
} PPB_Context3DTrustedError;

struct PP_Context3DTrustedState {
  // Size of the command buffer in command buffer entries.
  int32_t num_entries;

  // The offset (in entries) from which the reader is reading.
  int32_t get_offset;

  // The offset (in entries) at which the writer is writing.
  int32_t put_offset;

  // The current token value. This is used by the writer to defer
  // changes to shared memory objects until the reader has reached a certain
  // point in the command buffer. The reader is responsible for updating the
  // token value, for example in response to an asynchronous set token command
  // embedded in the command buffer. The default token value is zero.
  int32_t token;

  // Error status.
  PPB_Context3DTrustedError error;
};

struct PPB_Context3DTrusted_Dev {
  // Creates a raw Context3D resource. A raw Context3D is intended to be used
  // with the trusted interface, through the command buffer (for proxying). In
  // particular, when a Surface3D is bound to a raw context, SwapBuffers has no
  // effect.
  PP_Resource (*CreateRaw)(PP_Instance instance_id,
                           PP_Config3D_Dev config,
                           PP_Resource share_context,
                           const int32_t* attrib_list);

  // Initializes the command buffer with the given size.
  PP_Bool (*Initialize)(PP_Resource context_id, int32_t size);

  // Gets the ring buffer for the command buffer.
  PP_Bool (*GetRingBuffer)(PP_Resource context_id,
                           int* shm_handle,
                           uint32_t* shm_size);

  // Returns the current state.
  struct PP_Context3DTrustedState (*GetState)(PP_Resource context);

  // The writer calls this to update its put offset.
  PP_Bool (*Flush)(PP_Resource context, int32_t put_offset);


  // The writer calls this to update its put offset. This function returns the
  // reader's most recent get offset. Does not return until after the put offset
  // change callback has been invoked.
  struct PP_Context3DTrustedState (*FlushSync)(PP_Resource context,
                                               int32_t put_offset);

  // Create a transfer buffer and return a handle that uniquely
  // identifies it or -1 on error.
  int32_t (*CreateTransferBuffer)(PP_Resource context, size_t size);

  // Destroy a transfer buffer and recycle the handle.
  PP_Bool (*DestroyTransferBuffer)(PP_Resource context, int32_t id);

  // Get the transfer buffer associated with a handle.
  PP_Bool (*GetTransferBuffer)(PP_Resource context,
                               int32_t id,
                               int* shm_handle,
                               uint32_t* shm_size);
};

#endif  // PPAPI_C_DEV_PPB_CONTEXT_3D_TRUSTED_DEV_H_

