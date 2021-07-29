// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/bind.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/common/content_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using testing::Mock;
using testing::Return;

namespace content {

namespace {

const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
using PromptEventCallback =
    base::OnceCallback<void(BluetoothScanningPrompt::Event)>;

class FakeBluetoothScanningPrompt : public BluetoothScanningPrompt {
 public:
  FakeBluetoothScanningPrompt(PromptEventCallback prompt_event_callback)
      : prompt_event_callback_(std::move(prompt_event_callback)) {}
  ~FakeBluetoothScanningPrompt() override = default;

  // Move-only class.
  FakeBluetoothScanningPrompt(const FakeBluetoothScanningPrompt&) = delete;
  FakeBluetoothScanningPrompt& operator=(const FakeBluetoothScanningPrompt&) =
      delete;

  void RunPromptEventCallback(Event event) {
    if (prompt_event_callback_.is_null()) {
      FAIL() << "prompt_event_callback_ is not set";
    }
    std::move(prompt_event_callback_).Run(event);
  }

 private:
  PromptEventCallback prompt_event_callback_;
};

class FakeBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class.
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  // device::BluetoothAdapter:
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

 private:
  ~FakeBluetoothAdapter() override = default;
};

class TestBluetoothDelegate : public BluetoothDelegate {
 public:
  TestBluetoothDelegate() = default;
  ~TestBluetoothDelegate() override = default;
  TestBluetoothDelegate(const TestBluetoothDelegate&) = delete;
  TestBluetoothDelegate& operator=(const TestBluetoothDelegate&) = delete;

  // BluetoothDelegate:
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler) override {
    return nullptr;
  }
  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override {
    auto prompt =
        std::make_unique<FakeBluetoothScanningPrompt>(std::move(event_handler));
    prompt_ = prompt.get();
    return std::move(prompt);
  }
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      RenderFrameHost* frame,
      const std::string& device_address) override {
    return blink::WebBluetoothDeviceId();
  }
  std::string GetDeviceAddress(RenderFrameHost* frame,
                               const blink::WebBluetoothDeviceId&) override {
    return std::string();
  }
  blink::WebBluetoothDeviceId AddScannedDevice(
      RenderFrameHost* frame,
      const std::string& device_address) override {
    return blink::WebBluetoothDeviceId();
  }
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override {
    return blink::WebBluetoothDeviceId();
  }
  bool HasDevicePermission(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {
    return false;
  }
  bool IsAllowedToAccessService(RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override {
    return false;
  }
  bool IsAllowedToAccessAtLeastOneService(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {
    return false;
  }
  bool IsAllowedToAccessManufacturerData(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      const uint16_t manufacturer_code) override {
    return false;
  }
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      RenderFrameHost* frame) override {
    return {};
  }

  void RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event event) {
    if (!prompt_) {
      FAIL() << "ShowBluetoothScanningPrompt must be called before "
             << __func__;
    }
    prompt_->RunPromptEventCallback(event);
  }

  void AddFramePermissionObserver(FramePermissionObserver* observer) override {}
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override {}

 private:
  FakeBluetoothScanningPrompt* prompt_ = nullptr;
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  TestBluetoothDelegate* bluetooth_delegate() { return &bluetooth_delegate_; }

 protected:
  // ChromeContentBrowserClient:
  BluetoothDelegate* GetBluetoothDelegate() override {
    return &bluetooth_delegate_;
  }

 private:
  TestBluetoothDelegate bluetooth_delegate_;
};

class FakeWebBluetoothAdvertisementClientImpl
    : blink::mojom::WebBluetoothAdvertisementClient {
 public:
  FakeWebBluetoothAdvertisementClientImpl() = default;
  ~FakeWebBluetoothAdvertisementClientImpl() override = default;

  // Move-only class.
  FakeWebBluetoothAdvertisementClientImpl(
      const FakeWebBluetoothAdvertisementClientImpl&) = delete;
  FakeWebBluetoothAdvertisementClientImpl& operator=(
      const FakeWebBluetoothAdvertisementClientImpl&) = delete;

  // blink::mojom::WebBluetoothAdvertisementClient:
  void AdvertisingEvent(
      blink::mojom::WebBluetoothAdvertisingEventPtr event) override {}

  void BindReceiver(mojo::PendingAssociatedReceiver<
                    blink::mojom::WebBluetoothAdvertisementClient> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeWebBluetoothAdvertisementClientImpl::OnConnectionError,
        base::Unretained(this)));
  }

  void OnConnectionError() { on_connection_error_called_ = true; }

  bool on_connection_error_called() { return on_connection_error_called_; }

 private:
  mojo::AssociatedReceiver<blink::mojom::WebBluetoothAdvertisementClient>
      receiver_{this};
  bool on_connection_error_called_ = false;
};

}  // namespace

class WebBluetoothServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  WebBluetoothServiceImplTest() = default;
  ~WebBluetoothServiceImplTest() override = default;

  // Move-only class.
  WebBluetoothServiceImplTest(const WebBluetoothServiceImplTest&) = delete;
  WebBluetoothServiceImplTest& operator=(const WebBluetoothServiceImplTest&) =
      delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up an adapter.
    adapter_ = new FakeBluetoothAdapter();
    EXPECT_CALL(*adapter_, IsPresent()).WillRepeatedly(Return(true));
    BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
        adapter_);

    // Hook up the test bluetooth delegate.
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();

    // Simulate a frame connected to a bluetooth service.
    service_ =
        contents()->GetMainFrame()->CreateWebBluetoothServiceForTesting();
  }

  void TearDown() override {
    adapter_.reset();
    service_ = nullptr;
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  blink::mojom::WebBluetoothLeScanFilterPtr CreateScanFilter(
      const std::string& name,
      const std::string& name_prefix) {
    absl::optional<std::vector<device::BluetoothUUID>> services;
    services.emplace();
    services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
    return blink::mojom::WebBluetoothLeScanFilter::New(
        services, name, name_prefix, /*manufacturer_data=*/absl::nullopt);
  }

  blink::mojom::WebBluetoothResult RequestScanningStartAndSimulatePromptEvent(
      const blink::mojom::WebBluetoothLeScanFilter& filter,
      FakeWebBluetoothAdvertisementClientImpl* client_impl,
      BluetoothScanningPrompt::Event event) {
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client;
    client_impl->BindReceiver(client.InitWithNewEndpointAndPassReceiver());
    auto options = blink::mojom::WebBluetoothRequestLEScanOptions::New();
    options->filters.emplace();
    auto filter_ptr = blink::mojom::WebBluetoothLeScanFilter::New(
        filter.services, filter.name, filter.name_prefix,
        /*manufacturer_data=*/absl::nullopt);
    options->filters->push_back(std::move(filter_ptr));

    // Use two RunLoops to guarantee the order of operations for this test.
    // |callback_loop| guarantees that RequestScanningStartCallback has finished
    // executing and |result| has been populated. |request_loop| ensures that
    // the entire RequestScanningStart flow has finished before the method
    // returns.
    base::RunLoop callback_loop, request_loop;
    blink::mojom::WebBluetoothResult result;
    service_->RequestScanningStart(
        std::move(client), std::move(options),
        base::BindLambdaForTesting(
            [&callback_loop, &result](blink::mojom::WebBluetoothResult r) {
              result = std::move(r);
              callback_loop.Quit();
            }));

    // Post a task to simulate a prompt event during a call to
    // RequestScanningStart().
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(
                       [&callback_loop, &event, &request_loop, this]() {
                         browser_client_.bluetooth_delegate()
                             ->RunBluetoothScanningPromptEventCallback(event);
                         callback_loop.Run();
                         request_loop.Quit();
                       }));
    request_loop.Run();
    return result;
  }

  scoped_refptr<FakeBluetoothAdapter> adapter_;
  WebBluetoothServiceImpl* service_;
  TestContentBrowserClient browser_client_;
  ContentBrowserClient* old_browser_client_ = nullptr;
};

TEST_F(WebBluetoothServiceImplTest, ClearStateDuringRequestDevice) {
  auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
  options->accept_all_devices = true;

  base::RunLoop loop;
  service_->RequestDevice(
      std::move(options),
      base::BindLambdaForTesting(
          [&loop](blink::mojom::WebBluetoothResult,
                  blink::mojom::WebBluetoothDevicePtr) { loop.Quit(); }));
  service_->ClearState();
  loop.Run();
}

TEST_F(WebBluetoothServiceImplTest, PermissionAllowed) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));

  FakeWebBluetoothAdvertisementClientImpl client_impl;
  blink::mojom::WebBluetoothResult result =
      RequestScanningStartAndSimulatePromptEvent(
          *filter, &client_impl, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result, blink::mojom::WebBluetoothResult::SUCCESS);
  // |filters| should be allowed.
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest, ClearStateDuringRequestScanningStart) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters;

  FakeWebBluetoothAdvertisementClientImpl client_impl;
  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client;
  client_impl.BindReceiver(client.InitWithNewEndpointAndPassReceiver());

  auto options = blink::mojom::WebBluetoothRequestLEScanOptions::New();
  options->filters.emplace();
  options->filters->push_back(std::move(filter));

  // Use two RunLoops to guarantee the order of operations for this test.
  // |callback_loop| guarantees that RequestScanningStartCallback has finished
  // executing and |result| has been populated. |request_loop| ensures that the
  // entire RequestScanningStart flow has finished before |result| is checked.
  base::RunLoop callback_loop, request_loop;
  blink::mojom::WebBluetoothResult result;
  service_->RequestScanningStart(
      std::move(client), std::move(options),
      base::BindLambdaForTesting(
          [&callback_loop, &result](blink::mojom::WebBluetoothResult r) {
            result = std::move(r);
            callback_loop.Quit();
          }));

  // Post a task to clear the WebBluetoothService state during a call to
  // RequestScanningStart(). This should cause the RequestScanningStartCallback
  // to be run with an error result.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&callback_loop, this, &request_loop]() {
        service_->ClearState();
        callback_loop.Run();
        request_loop.Quit();
      }));
  request_loop.Run();

  EXPECT_NE(result, blink::mojom::WebBluetoothResult::SUCCESS);
}

TEST_F(WebBluetoothServiceImplTest, PermissionPromptCanceled) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));

  FakeWebBluetoothAdvertisementClientImpl client_impl;
  blink::mojom::WebBluetoothResult result =
      RequestScanningStartAndSimulatePromptEvent(
          *filter, &client_impl, BluetoothScanningPrompt::Event::kCanceled);

  EXPECT_EQ(blink::mojom::WebBluetoothResult::PROMPT_CANCELED, result);
  // |filters| should still not be allowed.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabHidden) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothAdvertisementClientImpl client_impl;
  blink::mojom::WebBluetoothResult result =
      RequestScanningStartAndSimulatePromptEvent(
          *filter, &client_impl, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result, blink::mojom::WebBluetoothResult::SUCCESS);
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));

  contents()->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabOccluded) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothAdvertisementClientImpl client_impl;
  RequestScanningStartAndSimulatePromptEvent(
      *filter, &client_impl, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));

  contents()->SetVisibilityAndNotifyObservers(Visibility::OCCLUDED);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenFocusIsLost) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothAdvertisementClientImpl client_impl;
  RequestScanningStartAndSimulatePromptEvent(
      *filter, &client_impl, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));

  main_test_rfh()->GetRenderWidgetHost()->LostFocus();

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenBlocked) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter_1 =
      CreateScanFilter("a", "b");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters_1;
  filters_1.emplace();
  filters_1->push_back(filter_1.Clone());
  FakeWebBluetoothAdvertisementClientImpl client_impl_1;
  blink::mojom::WebBluetoothResult result_1 =
      RequestScanningStartAndSimulatePromptEvent(
          *filter_1, &client_impl_1, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result_1, blink::mojom::WebBluetoothResult::SUCCESS);
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters_1));
  EXPECT_FALSE(client_impl_1.on_connection_error_called());

  blink::mojom::WebBluetoothLeScanFilterPtr filter_2 =
      CreateScanFilter("c", "d");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters_2;
  filters_2.emplace();
  filters_2->push_back(filter_2.Clone());
  FakeWebBluetoothAdvertisementClientImpl client_impl_2;
  blink::mojom::WebBluetoothResult result_2 =
      RequestScanningStartAndSimulatePromptEvent(
          *filter_2, &client_impl_2, BluetoothScanningPrompt::Event::kAllow);
  EXPECT_EQ(result_2, blink::mojom::WebBluetoothResult::SUCCESS);
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters_2));
  EXPECT_FALSE(client_impl_2.on_connection_error_called());

  blink::mojom::WebBluetoothLeScanFilterPtr filter_3 =
      CreateScanFilter("e", "f");
  absl::optional<WebBluetoothServiceImpl::ScanFilters> filters_3;
  filters_3.emplace();
  filters_3->push_back(filter_3.Clone());
  FakeWebBluetoothAdvertisementClientImpl client_impl_3;
  blink::mojom::WebBluetoothResult result_3 =
      RequestScanningStartAndSimulatePromptEvent(
          *filter_3, &client_impl_3, BluetoothScanningPrompt::Event::kBlock);
  EXPECT_EQ(blink::mojom::WebBluetoothResult::SCANNING_BLOCKED, result_3);
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_3));

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_1));
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_2));

  base::RunLoop().RunUntilIdle();

  // All existing scanning clients are disconnected.
  EXPECT_TRUE(client_impl_1.on_connection_error_called());
  EXPECT_TRUE(client_impl_2.on_connection_error_called());
  EXPECT_TRUE(client_impl_3.on_connection_error_called());
}

TEST_F(WebBluetoothServiceImplTest,
       ReadCharacteristicValueErrorWithValueIgnored) {
  // The contract for calls accepting a
  // BluetoothRemoteGattCharacteristic::ValueCallback callback argument is that
  // when an error occurs, value must be ignored. This test verifies that
  // WebBluetoothServiceImpl::OnCharacteristicReadValue honors that contract
  // and will not pass a value to it's callback
  // (a RemoteCharacteristicReadValueCallback instance) when an error occurs
  // with a non-empty value array.
  const std::vector<uint8_t> read_error_value = {1, 2, 3};
  bool callback_called = false;
  service_->OnCharacteristicReadValue(
      base::BindLambdaForTesting(
          [&callback_called](
              blink::mojom::WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            callback_called = true;
            EXPECT_EQ(
                blink::mojom::WebBluetoothResult::GATT_OPERATION_IN_PROGRESS,
                result);
            EXPECT_FALSE(value.has_value());
          }),
      device::BluetoothGattService::GATT_ERROR_IN_PROGRESS, read_error_value);
  EXPECT_TRUE(callback_called);

  // This test doesn't invoke any methods of the mock adapter. Allow it to be
  // leaked without producing errors.
  Mock::AllowLeak(adapter_.get());
}

}  // namespace content
