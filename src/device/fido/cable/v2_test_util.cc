// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_test_util.h"

#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/virtual_ctap2_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_network_context.h"
#include "url/gurl.h"

namespace device {
namespace cablev2 {
namespace {

// TestNetworkContext intercepts WebSocket creation calls and simulates a
// caBLEv2 tunnel server.
class TestNetworkContext : public network::TestNetworkContext {
 public:
  explicit TestNetworkContext(absl::optional<ContactCallback> contact_callback)
      : contact_callback_(std::move(contact_callback)) {}

  void CreateWebSocket(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      const net::SiteForCookies& site_for_cookies,
      const net::IsolationInfo& isolation_info,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      int32_t process_id,
      const url::Origin& origin,
      uint32_t options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          auth_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
      const absl::optional<base::UnguessableToken>& throttling_profile_id)
      override {
    CHECK(url.has_path());

    base::StringPiece path = url.path_piece();
    static const char kNewPrefix[] = "/cable/new/";
    static const char kConnectPrefix[] = "/cable/connect/";
    static const char kContactPrefix[] = "/cable/contact/";
    if (path.find(kNewPrefix) == 0) {
      path.remove_prefix(sizeof(kNewPrefix) - 1);
      CHECK(!base::Contains(connections_, std::string(path)));
      connections_.emplace(std::string(path), std::make_unique<Connection>(
                                                  Connection::Type::NEW,
                                                  std::move(handshake_client)));
    } else if (path.find(kConnectPrefix) == 0) {
      path.remove_prefix(sizeof(kConnectPrefix) - 1);
      // The first part of |path| will be a hex-encoded routing ID followed by a
      // '/'. Skip it.
      constexpr size_t kRoutingIdComponentSize = 2 * kRoutingIdSize + 1;
      CHECK_GE(path.size(), kRoutingIdComponentSize);
      path.remove_prefix(kRoutingIdComponentSize);

      const auto it = connections_.find(std::string(path));
      CHECK(it != connections_.end()) << "Unknown tunnel requested";
      it->second->set_peer(std::make_unique<Connection>(
          Connection::Type::CONNECT, std::move(handshake_client)));
    } else if (path.find(kContactPrefix) == 0) {
      path.remove_prefix(sizeof(kContactPrefix) - 1);

      CHECK_EQ(additional_headers.size(), 1u);
      CHECK_EQ(additional_headers[0]->name, device::kCableClientPayloadHeader);

      if (!contact_callback_) {
        // Without a contact callback all attempts are rejected with a 410
        // status to indicate the the contact ID will never work again.
        mojo::Remote<network::mojom::WebSocketHandshakeClient>
            bound_handshake_client(std::move(handshake_client));
        bound_handshake_client->OnFailure("", net::OK, net::HTTP_GONE);
        return;
      }

      std::vector<uint8_t> client_payload_bytes;
      CHECK(base::HexStringToBytes(additional_headers[0]->value,
                                   &client_payload_bytes));

      absl::optional<cbor::Value> client_payload =
          cbor::Reader::Read(client_payload_bytes);
      const cbor::Value::MapValue& map = client_payload->GetMap();

      uint8_t tunnel_id[kTunnelIdSize];
      crypto::RandBytes(tunnel_id);

      connections_.emplace(
          base::HexEncode(tunnel_id),
          std::make_unique<Connection>(Connection::Type::CONTACT,
                                       std::move(handshake_client)));

      const std::vector<uint8_t>& pairing_id_vec =
          map.find(cbor::Value(1))->second.GetBytestring();
      base::span<const uint8_t, kPairingIDSize> pairing_id(
          pairing_id_vec.data(), pairing_id_vec.size());

      const std::vector<uint8_t>& client_nonce_vec =
          map.find(cbor::Value(2))->second.GetBytestring();
      base::span<const uint8_t, kClientNonceSize> client_nonce(
          client_nonce_vec.data(), client_nonce_vec.size());

      const std::string& request_type_hint =
          map.find(cbor::Value(3))->second.GetString();

      contact_callback_->Run(tunnel_id, pairing_id, client_nonce,
                             request_type_hint);
    } else {
      CHECK(false) << "unexpected path: " << path;
    }
  }

 private:
  class Connection : public network::mojom::WebSocket {
   public:
    enum class Type {
      NEW,
      CONNECT,
      CONTACT,
    };

    Connection(Type type,
               mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
                   pending_handshake_client)
        : type_(type),
          in_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
          out_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
          handshake_client_(std::move(pending_handshake_client)) {
      MojoCreateDataPipeOptions options;
      memset(&options, 0, sizeof(options));
      options.struct_size = sizeof(options);
      options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
      options.element_num_bytes = sizeof(uint8_t);
      options.capacity_num_bytes = 1 << 16;

      CHECK_EQ(mojo::CreateDataPipe(&options, in_producer_, in_),
               MOJO_RESULT_OK);
      CHECK_EQ(mojo::CreateDataPipe(&options, out_, out_consumer_),
               MOJO_RESULT_OK);

      in_watcher_.Watch(in_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                        base::BindRepeating(&Connection::OnInPipeReady,
                                            base::Unretained(this)));
      out_watcher_.Watch(out_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                         MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                         base::BindRepeating(&Connection::OnOutPipeReady,
                                             base::Unretained(this)));
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&Connection::CompleteConnection,
                                    base::Unretained(this)));
    }

    void SendMessage(network::mojom::WebSocketMessageType type,
                     uint64_t length) override {
      if (!peer_ || !peer_->connected_) {
        pending_messages_.emplace_back(std::make_pair(type, length));
      } else {
        peer_->client_receiver_->OnDataFrame(/*final=*/true, type, length);
      }

      if (length > 0) {
        buffer_.resize(buffer_.size() + length);
        OnInPipeReady(MOJO_RESULT_OK, mojo::HandleSignalsState());
      }
    }

    void StartReceiving() override {}
    void StartClosingHandshake(uint16_t code,
                               const std::string& reason) override {
      CHECK(false);
    }

    void set_peer(std::unique_ptr<Connection> peer) {
      CHECK(!peer_);
      peer_ownership_ = std::move(peer);
      peer_ = peer_ownership_.get();
      peer_->set_nonowning_peer(this);

      Flush();
    }

   private:
    // name is useful when adding debugging messages. The first party to a
    // tunnel is "A" and the second is "B".
    const char* name() const {
      switch (type_) {
        case Type::NEW:
        case Type::CONTACT:
          return "A";
        case Type::CONNECT:
          return "B";
      }
    }

    void set_nonowning_peer(Connection* peer) {
      CHECK(!peer_);
      peer_ = peer;
      Flush();
    }

    void CompleteConnection() {
      CHECK(!connected_);
      auto response = network::mojom::WebSocketHandshakeResponse::New();
      response->selected_protocol = device::kCableWebSocketProtocol;

      if (type_ == Type::NEW) {
        auto header = network::mojom::HttpHeader::New();
        header->name = device::kCableRoutingIdHeader;
        std::array<uint8_t, kRoutingIdSize> routing_id = {42};
        header->value = base::HexEncode(routing_id);
        response->headers.push_back(std::move(header));
      }

      handshake_client_->OnConnectionEstablished(
          socket_.BindNewPipeAndPassRemote(),
          client_receiver_.BindNewPipeAndPassReceiver(), std::move(response),
          std::move(out_consumer_), std::move(in_producer_));

      connected_ = true;
      if (peer_) {
        peer_->Flush();
      }
    }

    void Flush() {
      if (!peer_->connected_) {
        return;
      }

      for (const auto& pending_message : pending_messages_) {
        peer_->client_receiver_->OnDataFrame(
            /*final=*/true, pending_message.first, pending_message.second);
      }

      if (!buffer_.empty()) {
        peer_->out_watcher_.Arm();
      }
    }

    void OnInPipeReady(MojoResult, const mojo::HandleSignalsState&) {
      const size_t todo = buffer_.size() - buffer_i_;
      CHECK_GT(todo, 0u);

      // We CHECK that the message fits into Mojo's 32-bit lengths because we
      // don't expect anything that large in unittests.
      uint32_t todo_32 = todo;
      CHECK_LE(todo, std::numeric_limits<decltype(todo_32)>::max());

      const MojoResult result = in_->ReadData(
          &buffer_.data()[buffer_i_], &todo_32, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_OK) {
        buffer_i_ += todo_32;
        CHECK_LE(buffer_i_, buffer_.size());

        if (peer_ && buffer_i_ > 0) {
          peer_->OnOutPipeReady(MOJO_RESULT_OK, mojo::HandleSignalsState());
        }

        if (buffer_i_ < buffer_.size()) {
          in_watcher_.Arm();
        } else {
          // TODO
        }
      } else if (result == MOJO_RESULT_SHOULD_WAIT) {
        in_watcher_.Arm();
      } else {
        CHECK(false) << static_cast<int>(result);
      }
    }

    void OnOutPipeReady(MojoResult, const mojo::HandleSignalsState&) {
      const size_t todo = peer_->buffer_.size();
      if (todo == 0) {
        return;
      }

      uint32_t todo_32 = todo;
      const MojoResult result = out_->WriteData(peer_->buffer_.data(), &todo_32,
                                                MOJO_WRITE_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_OK) {
        if (todo_32 == todo) {
          peer_->buffer_.clear();
          peer_->buffer_i_ = 0;
        } else {
          const size_t new_length = todo - todo_32;
          memmove(peer_->buffer_.data(), &peer_->buffer_.data()[todo_32],
                  new_length);
          peer_->buffer_.resize(new_length);
          peer_->buffer_i_ -= todo_32;
        }

        if (!peer_->buffer_.empty()) {
          out_watcher_.Arm();
        }
      } else if (result == MOJO_RESULT_SHOULD_WAIT) {
        out_watcher_.Arm();
      } else {
        CHECK(false) << static_cast<int>(result);
      }
    }

    const Type type_;
    bool connected_ = false;
    std::unique_ptr<Connection> peer_ownership_;
    std::vector<uint8_t> buffer_;
    std::vector<std::pair<network::mojom::WebSocketMessageType, uint64_t>>
        pending_messages_;
    size_t buffer_i_ = 0;
    mojo::SimpleWatcher in_watcher_;
    mojo::SimpleWatcher out_watcher_;
    Connection* peer_ = nullptr;
    mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client_;
    mojo::Remote<network::mojom::WebSocketClient> client_receiver_;
    mojo::Receiver<network::mojom::WebSocket> socket_{this};
    mojo::ScopedDataPipeConsumerHandle in_;
    mojo::ScopedDataPipeProducerHandle in_producer_;
    mojo::ScopedDataPipeProducerHandle out_;
    mojo::ScopedDataPipeConsumerHandle out_consumer_;
  };

  std::map<std::string, std::unique_ptr<Connection>> connections_;
  const absl::optional<ContactCallback> contact_callback_;
};

class DummyBLEAdvert
    : public device::cablev2::authenticator::Platform::BLEAdvert {};

// TestPlatform implements the platform support for caBLEv2 by forwarding
// messages to the given |VirtualCtap2Device|.
class TestPlatform : public authenticator::Platform {
 public:
  TestPlatform(Discovery::AdvertEventStream::Callback ble_advert_callback,
               device::VirtualCtap2Device* ctap2_device)
      : ble_advert_callback_(ble_advert_callback),
        ctap2_device_(ctap2_device) {}

  void MakeCredential(std::unique_ptr<MakeCredentialParams> params) override {
    std::vector<device::PublicKeyCredentialParams::CredentialInfo> cred_infos;
    for (const auto& algo : params->algorithms) {
      device::PublicKeyCredentialParams::CredentialInfo cred_info;
      cred_info.algorithm = algo;
      cred_infos.push_back(cred_info);
    }

    device::CtapMakeCredentialRequest request(
        /*client_data_json=*/"",
        device::PublicKeyCredentialRpEntity(params->rp_id),
        device::PublicKeyCredentialUserEntity(
            device::fido_parsing_utils::Materialize(params->user_id),
            /*name=*/absl::nullopt, /*display_name=*/absl::nullopt,
            /*icon_url=*/absl::nullopt),
        device::PublicKeyCredentialParams(std::move(cred_infos)));
    CHECK_EQ(request.client_data_hash.size(), params->client_data_hash.size());
    memcpy(request.client_data_hash.data(), params->client_data_hash.data(),
           params->client_data_hash.size());

    std::pair<device::CtapRequestCommand, absl::optional<cbor::Value>>
        request_cbor = AsCTAPRequestValuePair(request);

    ctap2_device_->DeviceTransact(
        ToCTAP2Command(std::move(request_cbor)),
        base::BindOnce(&TestPlatform::OnMakeCredentialResult,
                       weak_factory_.GetWeakPtr(),
                       std::move(params->callback)));
  }

  void GetAssertion(std::unique_ptr<GetAssertionParams> params) override {
    NOTREACHED();
  }

  void OnStatus(Status status) override {}
  void OnCompleted(absl::optional<Error> maybe_error) override {}

  std::unique_ptr<authenticator::Platform::BLEAdvert> SendBLEAdvert(
      base::span<const uint8_t, kAdvertSize> payload) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TestPlatform::DoSendBLEAdvert, weak_factory_.GetWeakPtr(),
            device::fido_parsing_utils::Materialize<EXTENT(payload)>(payload)));
    return std::make_unique<DummyBLEAdvert>();
  }

 private:
  void DoSendBLEAdvert(base::span<const uint8_t, kAdvertSize> advert) {
    ble_advert_callback_.Run(advert);
  }

  std::vector<uint8_t> ToCTAP2Command(
      const std::pair<device::CtapRequestCommand, absl::optional<cbor::Value>>&
          parts) {
    std::vector<uint8_t> ret;

    if (parts.second.has_value()) {
      absl::optional<std::vector<uint8_t>> cbor_bytes =
          cbor::Writer::Write(std::move(*parts.second));
      ret.swap(*cbor_bytes);
    }

    ret.insert(ret.begin(), static_cast<uint8_t>(parts.first));
    return ret;
  }

  void OnMakeCredentialResult(MakeCredentialCallback callback,
                              absl::optional<std::vector<uint8_t>> result) {
    if (!result || result->empty()) {
      std::move(callback).Run(
          static_cast<uint32_t>(device::CtapDeviceResponseCode::kCtap2ErrOther),
          base::span<const uint8_t>());
      return;
    }
    const base::span<const uint8_t> payload = *result;

    if (payload.size() == 1 ||
        payload[0] !=
            static_cast<uint8_t>(device::CtapDeviceResponseCode::kSuccess)) {
      std::move(callback).Run(payload[0], base::span<const uint8_t>());
      return;
    }

    absl::optional<cbor::Value> v = cbor::Reader::Read(payload.subspan(1));
    const cbor::Value::MapValue& in_map = v->GetMap();

    cbor::Value::MapValue out_map;
    out_map.emplace("fmt", in_map.find(cbor::Value(1))->second.GetString());
    out_map.emplace("authData",
                    in_map.find(cbor::Value(2))->second.GetBytestring());
    out_map.emplace("attStmt", in_map.find(cbor::Value(3))->second.GetMap());

    absl::optional<std::vector<uint8_t>> attestation_obj =
        cbor::Writer::Write(cbor::Value(std::move(out_map)));

    std::move(callback).Run(
        static_cast<uint32_t>(device::CtapDeviceResponseCode::kSuccess),
        *attestation_obj);
  }

  Discovery::AdvertEventStream::Callback ble_advert_callback_;
  device::VirtualCtap2Device* const ctap2_device_;
  base::WeakPtrFactory<TestPlatform> weak_factory_{this};
};

}  // namespace

std::unique_ptr<network::mojom::NetworkContext> NewMockTunnelServer(
    absl::optional<ContactCallback> contact_callback) {
  return std::make_unique<TestNetworkContext>(std::move(contact_callback));
}

namespace authenticator {

std::unique_ptr<authenticator::Platform> NewMockPlatform(
    Discovery::AdvertEventStream::Callback ble_advert_callback,
    device::VirtualCtap2Device* ctap2_device) {
  return std::make_unique<TestPlatform>(ble_advert_callback, ctap2_device);
}

}  // namespace authenticator

}  // namespace cablev2
}  // namespace device
