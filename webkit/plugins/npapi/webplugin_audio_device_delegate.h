// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_AUDIO_DEVICE_DELEGATE_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_AUDIO_DEVICE_DELEGATE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace webkit {
namespace npapi {

// Interface for the NPAPI audio device extension. This class implements "NOP"
// versions of all these functions so it can be used seamlessly by the
// "regular" plugin delegate while being overridden by the "pepper" one.
class WebPluginAudioDeviceDelegate {
 public:
  virtual NPError DeviceAudioQueryCapability(int32 capability, int32* value);
  virtual NPError DeviceAudioQueryConfig(
      const NPDeviceContextAudioConfig* request,
      NPDeviceContextAudioConfig* obtain);
  virtual NPError DeviceAudioInitializeContext(
      const NPDeviceContextAudioConfig* config,
      NPDeviceContextAudio* context);
  virtual NPError DeviceAudioSetStateContext(NPDeviceContextAudio* context,
                                             int32 state, intptr_t value);
  virtual NPError DeviceAudioGetStateContext(NPDeviceContextAudio* context,
                                             int32 state, intptr_t* value);
  virtual NPError DeviceAudioFlushContext(
      NPP id, NPDeviceContextAudio* context,
      NPDeviceFlushContextCallbackPtr callback, void* user_data);
  virtual NPError DeviceAudioDestroyContext(NPDeviceContextAudio* context);

 protected:
  WebPluginAudioDeviceDelegate() {}
  virtual ~WebPluginAudioDeviceDelegate() {}
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_AUDIO_DEVICE_DELEGATE_H_

