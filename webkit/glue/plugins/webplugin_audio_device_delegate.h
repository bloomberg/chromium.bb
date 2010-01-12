// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_WEBPLUGIN_AUDIO_DEVICE_DELEGATE_H_
#define WEBKIT_GLUE_PLUGINS_WEBPLUGIN_AUDIO_DEVICE_DELEGATE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace webkit_glue {

// Interface for the NPAPI audio device extension. This class implements "NOP"
// versions of all these functions so it can be used seamlessly by the
// "regular" plugin delegate while being overridden by the "pepper" one.
class WebPluginAudioDeviceDelegate {
 public:
  virtual NPError DeviceAudioQueryCapability(int32 capability, int32* value) {
    return NPERR_GENERIC_ERROR;
  }
  virtual NPError DeviceAudioQueryConfig(
      const NPDeviceContextAudioConfig* request,
      NPDeviceContextAudioConfig* obtain) {
    return NPERR_GENERIC_ERROR;
  }
  virtual NPError DeviceAudioInitializeContext(
      const NPDeviceContextAudioConfig* config,
      NPDeviceContextAudio* context) {
    return NPERR_GENERIC_ERROR;
  }
  virtual NPError DeviceAudioSetStateContext(NPDeviceContextAudio* context,
                                             int32 state, int32 value) {
    return NPERR_GENERIC_ERROR;
  }
  virtual NPError DeviceAudioGetStateContext(NPDeviceContextAudio* context,
                                             int32 state, int32* value) {
    return NPERR_GENERIC_ERROR;
  }
  virtual NPError DeviceAudioFlushContext(
      NPP id, NPDeviceContextAudio* context,
      NPDeviceFlushContextCallbackPtr callback, void* user_data) {
    return NPERR_GENERIC_ERROR;
  }
  virtual NPError DeviceAudioDestroyContext(NPDeviceContextAudio* context) {
    return NPERR_GENERIC_ERROR;
  }

 protected:
  WebPluginAudioDeviceDelegate() {}
  virtual ~WebPluginAudioDeviceDelegate() {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_PLUGINS_WEBPLUGIN_AUDIO_DEVICE_DELEGATE_H_

