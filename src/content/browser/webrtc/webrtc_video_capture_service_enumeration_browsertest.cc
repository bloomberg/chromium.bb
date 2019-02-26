// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace content {

namespace {

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const std::string kEnumerateVideoCaptureDevicesAndVerify =
    "enumerateVideoCaptureDevicesAndVerifyCount";

}  // anonymous namespace

// Integration test that obtains a connection to the video capture service via
// the Browser process' service manager. It then registers and unregisters
// virtual devices at the service and checks in JavaScript that the list of
// enumerated devices changes correspondingly.
class WebRtcVideoCaptureServiceEnumerationBrowserTest
    : public ContentBrowserTest {
 public:
  WebRtcVideoCaptureServiceEnumerationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
  }

  ~WebRtcVideoCaptureServiceEnumerationBrowserTest() override {}

  void ConnectToService() {
    connector_->BindInterface(video_capture::mojom::kServiceName, &provider_);
    provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory_));
  }

  void AddVirtualDevice(const std::string& device_id) {
    media::VideoCaptureDeviceInfo info;
    info.descriptor.device_id = device_id;
    info.descriptor.set_display_name(device_id);
    info.descriptor.capture_api = media::VideoCaptureApi::VIRTUAL_DEVICE;

    video_capture::mojom::TextureVirtualDevicePtr virtual_device;
    factory_->AddTextureVirtualDevice(info, mojo::MakeRequest(&virtual_device));
    virtual_devices_by_id_.insert(
        std::make_pair(device_id, std::move(virtual_device)));
  }

  void RemoveVirtualDevice(const std::string& device_id) {
    virtual_devices_by_id_.erase(device_id);
  }

  void DisconnectFromService() {
    factory_ = nullptr;
    provider_ = nullptr;
  }

  void EnumerateDevicesInRendererAndVerifyDeviceCount(
      int expected_device_count) {
    NavigateToURL(shell(),
                  GURL(embedded_test_server()->GetURL(kVideoCaptureHtmlFile)));

    const std::string javascript_to_execute = base::StringPrintf(
        (kEnumerateVideoCaptureDevicesAndVerify + "(%d)").c_str(),
        expected_device_count);
    std::string result;
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(shell(), javascript_to_execute, &result));
    ASSERT_EQ("OK", result);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Note: We are not planning to actually use any fake device, but we want
    // to avoid enumerating or otherwise calling into real capture devices.
    command_line->AppendSwitchASCII(switches::kUseFakeDeviceForMediaStream,
                                    "device-count=0");
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void Initialize() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();

    auto* connection = content::ServiceManagerConnection::GetForProcess();
    ASSERT_TRUE(connection);
    auto* connector = connection->GetConnector();
    ASSERT_TRUE(connector);
    connector_ = connector->Clone();
  }

  std::unique_ptr<service_manager::Connector> connector_;
  std::map<std::string, video_capture::mojom::TextureVirtualDevicePtr>
      virtual_devices_by_id_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  video_capture::mojom::DeviceFactoryProviderPtr provider_;
  video_capture::mojom::DeviceFactoryPtr factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCaptureServiceEnumerationBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcVideoCaptureServiceEnumerationBrowserTest,
                       SingleAddedVirtualDeviceGetsEnumerated) {
  Initialize();
  ConnectToService();

  // Exercise
  // TODO(chfremer): It is probably not guaranteed that the Mojo IPC call to
  // AddVirtualDevice arrives at the service before the request to enumerate
  // devices triggered by JavaScript. To guarantee this, we would have to add
  // a done-callback to AddVirtualDevice() and wait for that to arrive before
  // doing the enumeration.
  AddVirtualDevice("test");
  EnumerateDevicesInRendererAndVerifyDeviceCount(1);

  // Tear down
  RemoveVirtualDevice("test");
  DisconnectFromService();
}

IN_PROC_BROWSER_TEST_F(WebRtcVideoCaptureServiceEnumerationBrowserTest,
                       RemoveVirtualDeviceAfterItHasBeenEnumerated) {
  // TODO(chfremer): Remove this when https://crbug.com/876892 is resolved.
  if (base::FeatureList::IsEnabled(features::kMediaDevicesSystemMonitorCache)) {
    LOG(WARNING) << "Skipping test, because feature not yet supported when "
                    "device monitoring is enabled.";
    return;
  }
  Initialize();
  ConnectToService();

  AddVirtualDevice("test_1");
  AddVirtualDevice("test_2");
  EnumerateDevicesInRendererAndVerifyDeviceCount(2);
  RemoveVirtualDevice("test_1");
  EnumerateDevicesInRendererAndVerifyDeviceCount(1);
  RemoveVirtualDevice("test_2");
  EnumerateDevicesInRendererAndVerifyDeviceCount(0);

  // Tear down
  DisconnectFromService();
}

}  // namespace content
