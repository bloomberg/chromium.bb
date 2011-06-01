// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_audio_config_proxy.h"

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/audio_config_impl.h"
#include "ppapi/thunk/thunk.h"

namespace pp {
namespace proxy {

// The implementation is actually in AudioConfigImpl.
class AudioConfig : public PluginResource,
                    public ppapi::AudioConfigImpl {
 public:
  // Note that you must call Init (on AudioConfigImpl) before using this class.
  AudioConfig(const HostResource& resource);
  virtual ~AudioConfig();

  // ResourceObjectBase overrides.
  virtual ::ppapi::thunk::PPB_AudioConfig_API* AsAudioConfig_API() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioConfig);
};

AudioConfig::AudioConfig(const HostResource& resource)
    : PluginResource(resource) {
}

AudioConfig::~AudioConfig() {
}

::ppapi::thunk::PPB_AudioConfig_API* AudioConfig::AsAudioConfig_API() {
  return this;
}

namespace {

InterfaceProxy* CreateAudioConfigProxy(Dispatcher* dispatcher,
                                       const void* target_interface) {
  return new PPB_AudioConfig_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_AudioConfig_Proxy::PPB_AudioConfig_Proxy(Dispatcher* dispatcher,
                                             const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_AudioConfig_Proxy::~PPB_AudioConfig_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_AudioConfig_Proxy::GetInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_AudioConfig_Thunk(),
    PPB_AUDIO_CONFIG_INTERFACE,
    INTERFACE_ID_PPB_AUDIO_CONFIG,
    false,
    &CreateAudioConfigProxy,
  };
  return &info;
}

// static
PP_Resource PPB_AudioConfig_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  linked_ptr<AudioConfig> object(new AudioConfig(
      HostResource::MakeInstanceOnly(instance)));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

bool PPB_AudioConfig_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // There are no IPC messages for this interface.
  NOTREACHED();
  return false;
}

}  // namespace proxy
}  // namespace pp
