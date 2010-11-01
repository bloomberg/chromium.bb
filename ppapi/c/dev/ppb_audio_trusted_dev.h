// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_AUDIO_TRUSTED_DEV_H_
#define PPAPI_C_DEV_PPB_AUDIO_TRUSTED_DEV_H_

#include "ppapi/c/pp_resource.h"

#define PPB_AUDIO_TRUSTED_DEV_INTERFACE "PPB_AudioTrusted(Dev);0.1"

// This interface is used to get access to the audio buffer and a socket on
// which the client can block until the audio is ready to accept more data.
// This interface should be used by NaCl to implement the Audio interface.
struct PPB_AudioTrusted_Dev {
  // Returns a Buffer object that has the audio buffer.
  PP_Resource (*GetBuffer)(PP_Resource audio);

  // Returns a select()-able/Wait()-able OS-specific descriptor. The browser
  // will put a byte on the socket each time the buffer is ready to be filled.
  // The plugin can then implement its own audio thread using select()/poll() to
  // block until the browser is ready to receive data.
  int (*GetOSDescriptor)(PP_Resource audio);
};

#endif  // PPAPI_C_DEV_PPB_AUDIO_TRUSTED_DEV_H_

