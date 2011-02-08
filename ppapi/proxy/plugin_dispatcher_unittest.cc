// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"

namespace pp {
namespace proxy {

namespace {

bool received_create = false;

// Implement PPB_Audio since it's a relatively simple PPB interface and
// includes bidirectional communication.
PP_Resource Create(PP_Instance instance, PP_Resource config,
                   PPB_Audio_Callback audio_callback, void* user_data) {
  received_create = true;
  return 0;
}
PP_Bool IsAudio(PP_Resource resource) {
  return PP_FALSE;
}
PP_Resource GetCurrentConfig(PP_Resource audio) {
  return 0;
}
PP_Bool StartPlayback(PP_Resource audio) {
  return PP_FALSE;
}
PP_Bool StopPlayback(PP_Resource audio) {
  return PP_FALSE;
}

PPB_Audio dummy_audio_interface = {
  &Create,
  &IsAudio,
  &GetCurrentConfig,
  &StartPlayback,
  &StopPlayback
};

}  // namespace

class PluginDispatcherTest : public PluginProxyTest {
 public:
  PluginDispatcherTest() {}

  bool HasTargetProxy(InterfaceID id) {
    return !!plugin_dispatcher()->target_proxies_[id].get();
  }
};

TEST_F(PluginDispatcherTest, SupportsInterface) {
  RegisterTestInterface(PPB_AUDIO_INTERFACE, &dummy_audio_interface);
  RegisterTestInterface(PPP_INSTANCE_INTERFACE,
                        reinterpret_cast<void*>(0xdeadbeef));

  // Sending a request for a random interface should fail.
  EXPECT_FALSE(SupportsInterface("Random interface"));

  // Sending a request for a PPB interface should fail even though we've
  // registered it as existing in the GetInterface function (the plugin proxy
  // should only respond to PPP interfaces when asked if it supports them).
  EXPECT_FALSE(SupportsInterface(PPB_AUDIO_INTERFACE));

  // Sending a request for a supported PPP interface should succeed.
  EXPECT_TRUE(SupportsInterface(PPP_INSTANCE_INTERFACE));
}

TEST_F(PluginDispatcherTest, PPBCreation) {
  // Sending a PPB message out of the blue should create a target proxy for
  // that interface in the plugin.
  EXPECT_FALSE(HasTargetProxy(INTERFACE_ID_PPB_AUDIO));
  PpapiMsg_PPBAudio_NotifyAudioStreamCreated audio_msg(
      INTERFACE_ID_PPB_AUDIO, HostResource(), 0,
      IPC::PlatformFileForTransit(), base::SharedMemoryHandle(), 0);
  plugin_dispatcher()->OnMessageReceived(audio_msg);
  EXPECT_TRUE(HasTargetProxy(INTERFACE_ID_PPB_AUDIO));
}

}  // namespace proxy
}  // namespace pp

