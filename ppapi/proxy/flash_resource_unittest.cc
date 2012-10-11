// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest FlashResourceTest;

// This simulates the creation reply message of a VideoCapture resource. This
// won't be necessary once VideoCapture is converted to the new-style proxy.
class VideoCaptureCreationHandler : public IPC::Listener {
 public:
  VideoCaptureCreationHandler(ResourceMessageTestSink* test_sink,
                              PP_Instance instance)
      : test_sink_(test_sink),
        instance_(instance) {
  }
  virtual ~VideoCaptureCreationHandler() {}

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    if (msg.type() != ::PpapiHostMsg_PPBVideoCapture_Create::ID)
      return false;

    IPC::Message* reply_msg = IPC::SyncMessage::GenerateReply(&msg);
    HostResource resource;
    resource.SetHostResource(instance_, 12345);
    PpapiHostMsg_PPBVideoCapture_Create::WriteReplyParams(reply_msg, resource);
    test_sink_->SetSyncReplyMessage(reply_msg);
    return true;
  }
 private:
  ResourceMessageTestSink* test_sink_;
  PP_Instance instance_;
};

void* Unused(void* user_data, uint32_t element_count, uint32_t element_size) {
  return NULL;
}

}  // namespace

// Does a test of EnumerateVideoCaptureDevices() and reply functionality in
// the plugin side using the public C interfaces.
TEST_F(FlashResourceTest, EnumerateVideoCaptureDevices) {
  // TODO(raymes): This doesn't actually check that the data is converted from
  // |ppapi::DeviceRefData| to |PPB_DeviceRef| correctly, just that the right
  // messages are sent.

  // Set up a sync call handler that should return this message.
  std::vector<ppapi::DeviceRefData> reply_device_ref_data;
  int32_t expected_result = PP_OK;
  PpapiPluginMsg_Flash_EnumerateVideoCaptureDevicesReply reply_msg(
      reply_device_ref_data);
  ResourceSyncCallHandler enumerate_video_devices_handler(
      &sink(),
      PpapiHostMsg_Flash_EnumerateVideoCaptureDevices::ID,
      expected_result,
      reply_msg);
  sink().AddFilter(&enumerate_video_devices_handler);

  // Setup the handler to simulate creation of the video resource.
  VideoCaptureCreationHandler video_creation_handler(&sink(), pp_instance());
  sink().AddFilter(&video_creation_handler);

  // Set up the arguments to the call.
  ScopedPPResource video_capture(ScopedPPResource::PassRef(),
      ::ppapi::thunk::GetPPB_VideoCapture_Dev_0_2_Thunk()->Create(
          pp_instance()));
  std::vector<PP_Resource> unused;
  PP_ArrayOutput output;
  output.GetDataBuffer = &Unused;
  output.user_data = &unused;

  // Make the call.
  const PPB_Flash_12_6* flash_iface = ::ppapi::thunk::GetPPB_Flash_12_6_Thunk();
  int32_t actual_result = flash_iface->EnumerateVideoCaptureDevices(
      pp_instance(), video_capture.get(), output);

  // Check the result is as expected.
  EXPECT_EQ(expected_result, actual_result);

  // Should have sent an "EnumerateVideoCaptureDevices" message.
  ASSERT_TRUE(enumerate_video_devices_handler.last_handled_msg().type() ==
      PpapiHostMsg_Flash_EnumerateVideoCaptureDevices::ID);

  // Remove the filter or it will be destroyed before the sink() is destroyed.
  sink().RemoveFilter(&enumerate_video_devices_handler);
  sink().RemoveFilter(&video_creation_handler);
}

}  // namespace proxy
}  // namespace ppapi
