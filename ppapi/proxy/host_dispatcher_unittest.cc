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

class HostDispatcherTest : public HostProxyTest {
 public:
  HostDispatcherTest() {}

  bool HasTargetProxy(InterfaceID id) {
    return !!host_dispatcher()->target_proxies_[id].get();
  }
};

TEST_F(HostDispatcherTest, PPBCreation) {
  RegisterTestInterface(PPB_AUDIO_INTERFACE,
                        reinterpret_cast<void*>(0xdeadbeef));

  // Sending a PPB message out of the blue should create a target proxy for
  // that interface in the plugin.
  EXPECT_FALSE(HasTargetProxy(INTERFACE_ID_PPB_AUDIO));
  PpapiMsg_PPBAudio_NotifyAudioStreamCreated audio_msg(
      INTERFACE_ID_PPB_AUDIO, HostResource(), 0,
      IPC::PlatformFileForTransit(), base::SharedMemoryHandle(), 0);
  host_dispatcher()->OnMessageReceived(audio_msg);
  EXPECT_TRUE(HasTargetProxy(INTERFACE_ID_PPB_AUDIO));
}

}  // namespace proxy
}  // namespace pp

