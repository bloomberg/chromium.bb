// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_audio_device_delegate.h"

namespace webkit {
namespace npapi {

NPError WebPluginAudioDeviceDelegate::DeviceAudioQueryCapability(
    int32 capability, int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginAudioDeviceDelegate::DeviceAudioQueryConfig(
    const NPDeviceContextAudioConfig* request,
    NPDeviceContextAudioConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginAudioDeviceDelegate::DeviceAudioInitializeContext(
    const NPDeviceContextAudioConfig* config,
    NPDeviceContextAudio* context) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginAudioDeviceDelegate::DeviceAudioSetStateContext(
    NPDeviceContextAudio* context,
    int32 state, intptr_t value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginAudioDeviceDelegate::DeviceAudioGetStateContext(
    NPDeviceContextAudio* context,
    int32 state, intptr_t* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginAudioDeviceDelegate::DeviceAudioFlushContext(
    NPP id, NPDeviceContextAudio* context,
    NPDeviceFlushContextCallbackPtr callback, void* user_data) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginAudioDeviceDelegate::DeviceAudioDestroyContext(
    NPDeviceContextAudio* context) {
  return NPERR_GENERIC_ERROR;
}

}  // namespace npapi
}  // namespace webkit
