// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reader.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

using ::testing::_;
using ::testing::Invoke;

using device::mojom::blink::NDEFMessage;
using device::mojom::blink::NDEFMessagePtr;
using device::mojom::blink::NFCClientPtr;
using device::mojom::blink::NFCError;
using device::mojom::blink::NFCErrorPtr;
using device::mojom::blink::NFCErrorType;
using device::mojom::blink::NFCPushOptionsPtr;
using device::mojom::blink::NFCPushTarget;
using device::mojom::blink::NFCReaderOptions;
using device::mojom::blink::NFCReaderOptionsPtr;

static const char kTestUrl[] = "https://w3c.github.io/web-nfc/";

MATCHER_P(MessageEquals, expected, "") {
  // Only check the first data array.
  if (arg.data.size() != 1)
    return false;

  const auto& received_data = arg.data[0]->data;
  if (received_data.size() != expected.size())
    return false;

  for (WTF::wtf_size_t i = 0; i < received_data.size(); i++) {
    if (received_data[i] != expected[i]) {
      return false;
    }
  }
  return true;
}

class MockNFCReader : public GarbageCollectedFinalized<MockNFCReader>,
                      public NFCReader {
  USING_GARBAGE_COLLECTED_MIXIN(MockNFCReader);

 public:
  explicit MockNFCReader(NFCReaderOptionsPtr options)
      : NFCReader(std::move(options)) {}

  void Trace(blink::Visitor* visitor) override { NFCReader::Trace(visitor); }

  MOCK_METHOD1(OnMessage, void(const NDEFMessage& message));
  MOCK_METHOD1(OnReadingError, void(const NFCError& error));
};

class FakeNfcService : public device::mojom::blink::NFC {
 public:
  FakeNfcService() : binding_(this) {}
  ~FakeNfcService() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(device::mojom::blink::NFCRequest(std::move(handle)));
    binding_.set_connection_error_handler(
        WTF::Bind(&FakeNfcService::OnConnectionError, WTF::Unretained(this)));
  }

  void OnConnectionError() {
    if (binding_.is_bound())
      binding_.Unbind();

    client_.reset();
  }

  void TriggerWatchEvent() {
    if (!client_ || !tag_message_)
      return;

    // Only match the watches using |url| in options.
    WTF::Vector<uint32_t> ids;
    for (auto& pair : watches_) {
      if (pair.second->url == tag_message_->url) {
        ids.push_back(pair.first);
      }
    }

    if (!ids.IsEmpty()) {
      client_->OnWatch(std::move(ids), tag_message_.Clone());
    }
  }

  void set_tag_message(NDEFMessagePtr message) {
    tag_message_ = std::move(message);
  }

  WTF::Vector<uint32_t> GetWatches() {
    WTF::Vector<uint32_t> ids;
    for (auto& pair : watches_) {
      ids.push_back(pair.first);
    }
    return ids;
  }

 private:
  // Override methods from device::mojom::blink::NFC.
  void SetClient(NFCClientPtr client) override { client_ = std::move(client); }
  void Push(NDEFMessagePtr message,
            NFCPushOptionsPtr options,
            PushCallback callback) override {
    set_tag_message(std::move(message));
    std::move(callback).Run(nullptr);
  }
  void CancelPush(NFCPushTarget target, CancelPushCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void Watch(NFCReaderOptionsPtr options, WatchCallback callback) override {
    uint32_t id = ++last_watch_id_;
    watches_.insert(std::make_pair(id, std::move(options)));
    std::move(callback).Run(id, nullptr);
  }
  void CancelWatch(uint32_t id, CancelWatchCallback callback) override {
    if (watches_.erase(id) < 1) {
      std::move(callback).Run(NFCError::New(NFCErrorType::NOT_FOUND));
    } else {
      std::move(callback).Run(nullptr);
    }
  }
  void CancelAllWatches(CancelAllWatchesCallback callback) override {
    watches_.clear();
    std::move(callback).Run(nullptr);
  }
  void SuspendNFCOperations() override {}
  void ResumeNFCOperations() override {}

  NDEFMessagePtr tag_message_;
  NFCClientPtr client_;
  uint32_t last_watch_id_ = 0;
  std::map<uint32_t, NFCReaderOptionsPtr> watches_;
  mojo::Binding<device::mojom::blink::NFC> binding_;
};

// Overrides requests for NFC mojo requests with FakeNfcService instances.
class NFCProxyTest : public PageTestBase {
 public:
  NFCProxyTest() { nfc_service_ = std::make_unique<FakeNfcService>(); }

  void SetUp() override {
    PageTestBase::SetUp(IntSize());

    service_manager::InterfaceProvider::TestApi test_api(
        GetDocument().GetInterfaceProvider());
    test_api.SetBinderForName(
        device::mojom::blink::NFC::Name_,
        WTF::BindRepeating(&FakeNfcService::BindRequest,
                           WTF::Unretained(nfc_service())));
  }

  FakeNfcService* nfc_service() { return nfc_service_.get(); }

  void DestroyNfcService() { nfc_service_.reset(); }

 private:
  std::unique_ptr<FakeNfcService> nfc_service_;
};

TEST_F(NFCProxyTest, SuccessfulPath) {
  auto& document = GetDocument();
  auto* nfc_proxy = NFCProxy::From(document);
  NFCReaderOptionsPtr read_options = NFCReaderOptions::New();
  read_options->url = kTestUrl;
  auto* reader = MakeGarbageCollected<MockNFCReader>(std::move(read_options));

  nfc_proxy->AddReader(reader);
  test::RunPendingTasks();
  EXPECT_EQ(nfc_service()->GetWatches().size(), 1u);

  // Construct a NDEFMessagePtr
  auto message = device::mojom::blink::NDEFMessage::New();
  message->url = kTestUrl;
  auto record = device::mojom::blink::NDEFRecord::New();
  WTF::Vector<uint8_t> record_data(
      {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10});
  record->record_type = device::mojom::blink::NDEFRecordType::OPAQUE_RECORD;
  record->data = WTF::Vector<uint8_t>(record_data);
  message->data.push_back(std::move(record));

  base::RunLoop loop;
  EXPECT_CALL(*reader, OnMessage(MessageEquals(record_data)))
      .WillOnce(Invoke([&](const NDEFMessage& message) { loop.Quit(); }));

  nfc_proxy->Push(std::move(message), /*options=*/nullptr,
                  base::BindLambdaForTesting([&](NFCErrorPtr error) {
                    nfc_service()->TriggerWatchEvent();
                  }));
  loop.Run();

  nfc_proxy->RemoveReader(reader);
  test::RunPendingTasks();
  EXPECT_EQ(nfc_service()->GetWatches().size(), 0u);
}

TEST_F(NFCProxyTest, ErrorPath) {
  auto& document = GetDocument();
  auto* nfc_proxy = NFCProxy::From(document);
  NFCReaderOptionsPtr read_options = NFCReaderOptions::New();
  read_options->url = kTestUrl;
  auto* reader = MakeGarbageCollected<MockNFCReader>(std::move(read_options));

  nfc_proxy->AddReader(reader);
  test::RunPendingTasks();

  base::RunLoop loop;
  EXPECT_CALL(*reader, OnReadingError(_)).WillOnce(Invoke([&](const NFCError&) {
    loop.Quit();
  }));
  DestroyNfcService();
  loop.Run();
}

}  // namespace
}  // namespace blink
