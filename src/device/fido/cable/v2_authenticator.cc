// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_authenticator.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device {
namespace cablev2 {
namespace authenticator {

using device::CtapDeviceResponseCode;
using device::CtapRequestCommand;
using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::Map;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;
using device::cbor_extract::StringKey;

namespace {

// kTimeoutSeconds is the timeout that is put into the parameters that are
// passed up to the platform.
const int kTimeoutSeconds = 60;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cablev2_websocket_from_authenticator",
                                        R"(semantics {
          sender: "Phone as a Security Key"
          description:
            "Chrome on a phone can communicate with other devices for the "
            "purpose of using the phone as a security key. This WebSocket "
            "connection is made to a Google service that aids in the exchange "
            "of data with the other device. The service carries only "
            "end-to-end encrypted data where the keys are shared directly "
            "between the two devices via QR code and Bluetooth broadcast."
          trigger:
            "The user scans a QR code, displayed on the other device, and "
            "confirms their desire to communicate with it."
          data: "Only encrypted data that the service does not have the keys "
                "for."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "Not controlled by a setting because the operation is "
            "triggered by significant user action."
          policy_exception_justification:
            "No policy provided because the operation is triggered by "
            " significant user action. No background activity occurs."
        })");

struct MakeCredRequest {
  // All fields below are not a raw_ptr<T> because cbor_extract.cc would
  // cast the raw_ptr<T> to a void*, skipping an AddRef() call and causing a
  // ref-counting mismatch.
  const std::vector<uint8_t>* client_data_hash;
  const std::string* rp_id;
  const std::string* rp_name;
  const std::vector<uint8_t>* user_id;
  const std::string* user_name;
  const std::string* user_display_name;
  const cbor::Value::ArrayValue* cred_params;
  const cbor::Value::ArrayValue* excluded_credentials;
  const bool* resident_key;
};

static constexpr StepOrByte<MakeCredRequest> kMakeCredParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, MakeCredRequest, client_data_hash),
    IntKey<MakeCredRequest>(1),

    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(2),
      ELEMENT(Is::kRequired, MakeCredRequest, rp_id),
      StringKey<MakeCredRequest>(), 'i', 'd', '\0',

      ELEMENT(Is::kRequired, MakeCredRequest, rp_name),
      StringKey<MakeCredRequest>(), 'n', 'a', 'm', 'e', '\0',
    Stop<MakeCredRequest>(),

    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(3),
      ELEMENT(Is::kRequired, MakeCredRequest, user_id),
      StringKey<MakeCredRequest>(), 'i', 'd', '\0',

      ELEMENT(Is::kRequired, MakeCredRequest, user_name),
      StringKey<MakeCredRequest>(), 'n', 'a', 'm', 'e', '\0',

      ELEMENT(Is::kRequired, MakeCredRequest, user_display_name),
      StringKey<MakeCredRequest>(), 'd', 'i', 's', 'p', 'l', 'a', 'y',
                                    'N', 'a', 'm', 'e', '\0',
    Stop<MakeCredRequest>(),

    ELEMENT(Is::kRequired, MakeCredRequest, cred_params),
    IntKey<MakeCredRequest>(4),
    ELEMENT(Is::kOptional, MakeCredRequest, excluded_credentials),
    IntKey<MakeCredRequest>(5),

    Map<MakeCredRequest>(Is::kOptional),
    IntKey<MakeCredRequest>(7),
      ELEMENT(Is::kOptional, MakeCredRequest, resident_key),
      StringKey<MakeCredRequest>(), 'r', 'k', '\0',
    Stop<MakeCredRequest>(),

    Stop<MakeCredRequest>(),
    // clang-format on
};

struct AttestationObject {
  // All fields below are not a raw_ptr<T> because cbor_extract.cc would
  // cast the raw_ptr<T> to a void*, skipping an AddRef() call and causing a
  // ref-counting mismatch.
  const std::string* fmt;
  const std::vector<uint8_t>* auth_data;
  const cbor::Value* statement;
};

static constexpr StepOrByte<AttestationObject> kAttObjParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, AttestationObject, fmt),
    StringKey<AttestationObject>(), 'f', 'm', 't', '\0',

    ELEMENT(Is::kRequired, AttestationObject, auth_data),
    StringKey<AttestationObject>(), 'a', 'u', 't', 'h', 'D', 'a', 't', 'a',
                                    '\0',

    ELEMENT(Is::kRequired, AttestationObject, statement),
    StringKey<AttestationObject>(), 'a', 't', 't', 'S', 't', 'm', 't', '\0',
    Stop<AttestationObject>(),
    // clang-format on
};

struct GetAssertionRequest {
  // All fields below are not a raw_ptr<T> because cbor_extract.cc would
  // cast the raw_ptr<T> to a void*, skipping an AddRef() call and causing a
  // ref-counting mismatch.
  const std::string* rp_id;
  const std::vector<uint8_t>* client_data_hash;
  const cbor::Value::ArrayValue* allowed_credentials;
};

static constexpr StepOrByte<GetAssertionRequest> kGetAssertionParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, GetAssertionRequest, rp_id),
    IntKey<GetAssertionRequest>(1),

    ELEMENT(Is::kRequired, GetAssertionRequest, client_data_hash),
    IntKey<GetAssertionRequest>(2),

    ELEMENT(Is::kOptional, GetAssertionRequest, allowed_credentials),
    IntKey<GetAssertionRequest>(3),

    Stop<GetAssertionRequest>(),
    // clang-format on
};

// BuildGetInfoResponse returns a CBOR-encoded getInfo response.
std::vector<uint8_t> BuildGetInfoResponse() {
  std::array<uint8_t, device::kAaguidLength> aaguid{};
  std::vector<cbor::Value> versions;
  versions.emplace_back("FIDO_2_0");

  cbor::Value::MapValue options;
  // This code is only invoked if a screen-lock (i.e. user verification) is
  // configured on the device. Therefore the 'uv' option is unconditionally
  // true.
  options.emplace("uv", true);
  options.emplace("rk",
                  base::FeatureList::IsEnabled(device::kWebAuthCableDisco));

  cbor::Value::MapValue response_map;
  response_map.emplace(1, std::move(versions));
  response_map.emplace(3, aaguid);
  response_map.emplace(4, std::move(options));

  return cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
}

std::array<uint8_t, device::cablev2::kNonceSize> RandomNonce() {
  std::array<uint8_t, device::cablev2::kNonceSize> ret;
  crypto::RandBytes(ret);
  return ret;
}

using GeneratePairingDataCallback =
    base::OnceCallback<absl::optional<cbor::Value>(
        base::span<const uint8_t, device::kP256X962Length> peer_public_key_x962,
        device::cablev2::HandshakeHash)>;

// TunnelTransport is a transport that uses WebSockets to talk to a cloud
// service and uses BLE adverts to show proximity.
class TunnelTransport : public Transport {
 public:
  TunnelTransport(
      Platform* platform,
      network::mojom::NetworkContext* network_context,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, device::kP256X962Length> peer_identity,
      bool use_new_crypter_construction,
      GeneratePairingDataCallback generate_pairing_data)
      : platform_(platform),
        tunnel_id_(device::cablev2::Derive<EXTENT(tunnel_id_)>(
            secret,
            base::span<uint8_t>(),
            DerivedValueType::kTunnelID)),
        eid_key_(device::cablev2::Derive<EXTENT(eid_key_)>(
            secret,
            base::span<const uint8_t>(),
            device::cablev2::DerivedValueType::kEIDKey)),
        network_context_(network_context),
        peer_identity_(device::fido_parsing_utils::Materialize(peer_identity)),
        generate_pairing_data_(std::move(generate_pairing_data)),
        secret_(fido_parsing_utils::Materialize(secret)),
        use_new_crypter_construction_(use_new_crypter_construction) {
    DCHECK_EQ(state_, State::kNone);
    state_ = State::kConnecting;

    websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
        base::BindOnce(&TunnelTransport::OnTunnelReady, base::Unretained(this)),
        base::BindRepeating(&TunnelTransport::OnTunnelData,
                            base::Unretained(this)));
    target_ = device::cablev2::tunnelserver::GetNewTunnelURL(kTunnelServer,
                                                             tunnel_id_);
  }

  TunnelTransport(
      Platform* platform,
      network::mojom::NetworkContext* network_context,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id,
      base::span<const uint8_t, 16> tunnel_id,
      bssl::UniquePtr<EC_KEY> local_identity)
      : platform_(platform),
        tunnel_id_(fido_parsing_utils::Materialize(tunnel_id)),
        eid_key_(device::cablev2::Derive<EXTENT(eid_key_)>(
            secret,
            client_nonce,
            device::cablev2::DerivedValueType::kEIDKey)),
        network_context_(network_context),
        secret_(fido_parsing_utils::Materialize(secret)),
        local_identity_(std::move(local_identity)),
        use_new_crypter_construction_(false) {
    DCHECK_EQ(state_, State::kNone);

    state_ = State::kConnectingPaired;

    websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
        base::BindOnce(&TunnelTransport::OnTunnelReady, base::Unretained(this)),
        base::BindRepeating(&TunnelTransport::OnTunnelData,
                            base::Unretained(this)));
    target_ = device::cablev2::tunnelserver::GetConnectURL(
        kTunnelServer, routing_id, tunnel_id);
  }

  // Transport:

  void StartReading(
      base::RepeatingCallback<void(Update)> update_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!update_callback_);

    update_callback_ = std::move(update_callback);

    // Delay the WebSocket creation by 250ms. This to measure whether DNS
    // errors are reduced in UMA stats. If so, then the network errors that we
    // see are probably due to a start-up race.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TunnelTransport::StartWebSocket,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(250));
  }

  void Write(std::vector<uint8_t> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(state_, kReady);

    if (!crypter_->Encrypt(&data)) {
      FIDO_LOG(ERROR) << "Failed to encrypt response";
      return;
    }
    websocket_client_->Write(data);
  }

 private:
  enum State {
    kNone,
    kConnecting,
    kConnectingPaired,
    kConnected,
    kConnectedPaired,
    kReady,
  };

  void StartWebSocket() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    network_context_->CreateWebSocket(
        target_, {device::kCableWebSocketProtocol}, net::SiteForCookies(),
        net::IsolationInfo(), /*additional_headers=*/{},
        network::mojom::kBrowserProcessId, url::Origin::Create(target_),
        network::mojom::kWebSocketOptionBlockAllCookies,
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
        websocket_client_->BindNewHandshakeClientPipe(),
        /*url_loader_network_observer=*/mojo::NullRemote(),
        /*auth_handler=*/mojo::NullRemote(),
        /*header_client=*/mojo::NullRemote(),
        /*throttling_profile_id=*/absl::nullopt);
    FIDO_LOG(DEBUG) << "Creating WebSocket to " << target_.spec();
  }

  void OnTunnelReady(
      WebSocketAdapter::Result result,
      absl::optional<std::array<uint8_t, device::cablev2::kRoutingIdSize>>
          routing_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(state_ == State::kConnecting || state_ == State::kConnectingPaired);
    bool ok = (result == WebSocketAdapter::Result::OK);

    if (ok && state_ == State::kConnecting && !routing_id) {
      FIDO_LOG(ERROR) << "Tunnel server did not specify routing ID";
      ok = false;
    }

    if (!ok) {
      FIDO_LOG(ERROR) << "Failed to connect to tunnel server";
      update_callback_.Run(Platform::Error::TUNNEL_SERVER_CONNECT_FAILED);
      return;
    }

    FIDO_LOG(DEBUG) << "WebSocket connection established.";

    CableEidArray plaintext_eid;
    if (state_ == State::kConnecting) {
      device::cablev2::eid::Components components;
      components.tunnel_server_domain = kTunnelServer;
      components.routing_id = *routing_id;
      components.nonce = RandomNonce();

      plaintext_eid = device::cablev2::eid::FromComponents(components);
      state_ = State::kConnected;
    } else {
      DCHECK_EQ(state_, State::kConnectingPaired);
      crypto::RandBytes(plaintext_eid);
      // The first byte is reserved to ensure that the format can be changed in
      // the future.
      plaintext_eid[0] = 0;
      state_ = State::kConnectedPaired;
    }

    ble_advert_ =
        platform_->SendBLEAdvert(eid::Encrypt(plaintext_eid, eid_key_));
    psk_ = device::cablev2::Derive<EXTENT(psk_)>(
        secret_, plaintext_eid, device::cablev2::DerivedValueType::kPSK);

    update_callback_.Run(Platform::Status::TUNNEL_SERVER_CONNECT);
  }

  void OnTunnelData(absl::optional<base::span<const uint8_t>> msg) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!msg) {
      FIDO_LOG(DEBUG) << "WebSocket tunnel closed";
      update_callback_.Run(Disconnected::kDisconnected);
      return;
    }

    switch (state_) {
      case State::kConnectedPaired:
      case State::kConnected: {
        std::vector<uint8_t> response;
        HandshakeResult result = RespondToHandshake(
            psk_, std::move(local_identity_), peer_identity_, *msg, &response);
        if (!result) {
          FIDO_LOG(ERROR) << "caBLE handshake failure";
          update_callback_.Run(Platform::Error::HANDSHAKE_FAILED);
          return;
        }
        FIDO_LOG(DEBUG) << "caBLE handshake complete";
        update_callback_.Run(Platform::Status::HANDSHAKE_COMPLETE);
        websocket_client_->Write(response);
        crypter_ = std::move(result->first);
        if (use_new_crypter_construction_) {
          crypter_->UseNewConstruction();
        }

        cbor::Value::MapValue post_handshake_msg;
        post_handshake_msg.emplace(1, BuildGetInfoResponse());

        if (state_ == State::kConnected) {
          absl::optional<cbor::Value> pairing_data(
              std::move(generate_pairing_data_)
                  .Run(*peer_identity_, result->second));
          if (pairing_data) {
            post_handshake_msg.emplace(2, std::move(*pairing_data));
          }
        }

        absl::optional<std::vector<uint8_t>> post_handshake_msg_bytes(
            EncodePaddedCBORMap(std::move(post_handshake_msg)));
        if (!post_handshake_msg_bytes) {
          FIDO_LOG(ERROR) << "failed to encode post-handshake message";
          return;
        }

        // All post-handshake messages should fit into the same padding bucket.
        // It doesn't have to be the smallest one, but that's currently true
        // which yields this easy check:
        DCHECK_EQ(post_handshake_msg_bytes->size(),
                  kPostHandshakeMsgPaddingGranularity);

        if (!crypter_->Encrypt(&post_handshake_msg_bytes.value())) {
          FIDO_LOG(ERROR) << "failed to encrypt post-handshake message";
          return;
        }
        websocket_client_->Write(*post_handshake_msg_bytes);

        state_ = State::kReady;
        break;
      }

      case State::kReady: {
        std::vector<uint8_t> plaintext;
        if (!crypter_->Decrypt(*msg, &plaintext)) {
          FIDO_LOG(ERROR) << "failed to decrypt caBLE message";
          update_callback_.Run(Platform::Error::DECRYPT_FAILURE);
          return;
        }

        if (first_message_) {
          update_callback_.Run(Platform::Status::REQUEST_RECEIVED);
          first_message_ = false;
        }
        update_callback_.Run(std::move(plaintext));
        break;
      }

      default:
        NOTREACHED();
    }
  }

  const raw_ptr<Platform> platform_;
  State state_ = State::kNone;
  const std::array<uint8_t, kTunnelIdSize> tunnel_id_;
  const std::array<uint8_t, kEIDKeySize> eid_key_;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  std::unique_ptr<Crypter> crypter_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const absl::optional<std::array<uint8_t, kP256X962Length>> peer_identity_;
  std::array<uint8_t, kPSKSize> psk_;
  GeneratePairingDataCallback generate_pairing_data_;
  const std::vector<uint8_t> secret_;
  bssl::UniquePtr<EC_KEY> local_identity_;
  const bool use_new_crypter_construction_;
  GURL target_;
  std::unique_ptr<Platform::BLEAdvert> ble_advert_;
  base::RepeatingCallback<void(Update)> update_callback_;
  bool first_message_ = true;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TunnelTransport> weak_factory_{this};
};

class CTAP2Processor : public Transaction {
 public:
  CTAP2Processor(std::unique_ptr<Transport> transport,
                 std::unique_ptr<Platform> platform)
      : transport_(std::move(transport)), platform_(std::move(platform)) {
    transport_->StartReading(base::BindRepeating(
        &CTAP2Processor::OnTransportUpdate, base::Unretained(this)));
  }

 private:
  void OnTransportUpdate(Transport::Update update) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (auto* error = absl::get_if<Platform::Error>(&update)) {
      platform_->OnCompleted(*error);
      return;
    } else if (auto* status = absl::get_if<Platform::Status>(&update)) {
      platform_->OnStatus(*status);
      return;
    } else if (absl::get_if<Transport::Disconnected>(&update)) {
      absl::optional<Platform::Error> maybe_error;
      if (!transaction_received_) {
        maybe_error = Platform::Error::UNEXPECTED_EOF;
      } else if (!transaction_done_) {
        maybe_error = Platform::Error::EOF_WHILE_PROCESSING;
      }
      platform_->OnCompleted(maybe_error);
      return;
    }

    std::vector<uint8_t>& msg = absl::get<std::vector<uint8_t>>(update);
    absl::optional<std::vector<uint8_t>> response = ProcessCTAPMessage(msg);
    if (!response) {
      // TODO(agl): expose more error information from |ProcessCTAPMessage|.
      platform_->OnCompleted(Platform::Error::INVALID_CTAP);
      return;
    }

    if (response->empty()) {
      // Response is pending.
      return;
    }

    transport_->Write(std::move(*response));
  }

  absl::optional<std::vector<uint8_t>> ProcessCTAPMessage(
      base::span<const uint8_t> message_bytes) {
    if (message_bytes.empty()) {
      return absl::nullopt;
    }
    const auto command = message_bytes[0];
    const auto cbor_bytes = message_bytes.subspan(1);

    absl::optional<cbor::Value> payload;
    if (!cbor_bytes.empty()) {
      payload = cbor::Reader::Read(cbor_bytes);
      if (!payload) {
        FIDO_LOG(ERROR) << "CBOR decoding failed for "
                        << base::HexEncode(cbor_bytes);
        return absl::nullopt;
      }
      FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(&command, 1) << ") "
                      << cbor::DiagnosticWriter::Write(*payload);
    } else {
      FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(&command, 1)
                      << ") <no payload>";
    }

    switch (command) {
      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorGetInfo): {
        if (payload) {
          FIDO_LOG(ERROR) << "getInfo command incorrectly contained payload";
          return absl::nullopt;
        }

        absl::optional<std::vector<uint8_t>> response = BuildGetInfoResponse();
        if (!response) {
          return absl::nullopt;
        }
        response->insert(
            response->begin(),
            static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
        return response;
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorMakeCredential): {
        if (!payload || !payload->is_map()) {
          FIDO_LOG(ERROR) << "Invalid makeCredential payload";
          return absl::nullopt;
        }

        MakeCredRequest make_cred_request;
        if (!device::cbor_extract::Extract<MakeCredRequest>(
                &make_cred_request, kMakeCredParseSteps, payload->GetMap())) {
          FIDO_LOG(ERROR) << "Failed to parse makeCredential request: "
                          << base::HexEncode(cbor_bytes);
          return absl::nullopt;
        }

        auto params = blink::mojom::PublicKeyCredentialCreationOptions::New();
        params->challenge = *make_cred_request.client_data_hash;
        params->timeout = base::Seconds(kTimeoutSeconds);

        params->relying_party.id = *make_cred_request.rp_id;
        params->relying_party.name = *make_cred_request.rp_name;

        params->user.id = *make_cred_request.user_id;
        params->user.name = *make_cred_request.user_name;
        params->user.display_name = *make_cred_request.user_display_name;

        params->authenticator_selection.emplace(
            device::AuthenticatorAttachment::kPlatform,
            (make_cred_request.resident_key && *make_cred_request.resident_key)
                ? device::ResidentKeyRequirement::kRequired
                : device::ResidentKeyRequirement::kDiscouraged,
            device::UserVerificationRequirement::kRequired);

        if (!CopyCredIds(make_cred_request.excluded_credentials,
                         &params->exclude_credentials)) {
          return absl::nullopt;
        }

        if (!device::cbor_extract::ForEachPublicKeyEntry(
                *make_cred_request.cred_params, cbor::Value("alg"),
                base::BindRepeating(
                    [](std::vector<
                           device::PublicKeyCredentialParams::CredentialInfo>*
                           out,
                       const cbor::Value& value) -> bool {
                      if (!value.is_integer()) {
                        return false;
                      }
                      const int64_t alg = value.GetInteger();

                      if (alg > std::numeric_limits<int32_t>::max() ||
                          alg < std::numeric_limits<int32_t>::min()) {
                        // This value cannot be represented in the `int32_t`
                        // in the Mojo structure and thus is ignored.
                        return true;
                      }
                      device::PublicKeyCredentialParams::CredentialInfo info;
                      info.algorithm = static_cast<int32_t>(alg);
                      out->push_back(info);
                      return true;
                    },
                    base::Unretained(&params->public_key_parameters)))) {
          return absl::nullopt;
        }

        transaction_received_ = true;
        platform_->MakeCredential(
            std::move(params),
            base::BindOnce(&CTAP2Processor::OnMakeCredentialResponse,
                           weak_factory_.GetWeakPtr()));
        return std::vector<uint8_t>();
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorGetAssertion): {
        if (!payload || !payload->is_map()) {
          FIDO_LOG(ERROR) << "Invalid makeCredential payload";
          return absl::nullopt;
        }

        GetAssertionRequest get_assertion_request;
        if (!device::cbor_extract::Extract<GetAssertionRequest>(
                &get_assertion_request, kGetAssertionParseSteps,
                payload->GetMap())) {
          FIDO_LOG(ERROR) << "Failed to parse getAssertion request";
          return absl::nullopt;
        }

        auto params = blink::mojom::PublicKeyCredentialRequestOptions::New();
        params->challenge = *get_assertion_request.client_data_hash;
        params->relying_party_id = *get_assertion_request.rp_id;
        params->user_verification =
            device::UserVerificationRequirement::kRequired;
        params->timeout = base::Seconds(kTimeoutSeconds);

        if (!CopyCredIds(get_assertion_request.allowed_credentials,
                         &params->allow_credentials)) {
          return absl::nullopt;
        }

        transaction_received_ = true;
        get_assertion_had_empty_allowlist_ = params->allow_credentials.empty();
        platform_->GetAssertion(
            std::move(params),
            base::BindOnce(&CTAP2Processor::OnGetAssertionResponse,
                           weak_factory_.GetWeakPtr()));
        return std::vector<uint8_t>();
      }

      default:
        FIDO_LOG(ERROR) << "Received unknown command "
                        << static_cast<unsigned>(command);
        return absl::nullopt;
    }
  }

  void OnMakeCredentialResponse(
      uint32_t ctap_status,
      base::span<const uint8_t> attestation_object_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_LE(ctap_status, 0xFFu);

    std::vector<uint8_t> response = {base::checked_cast<uint8_t>(ctap_status)};
    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      // TODO: pass response parameters from the Java side.
      absl::optional<cbor::Value> cbor_attestation_object =
          cbor::Reader::Read(attestation_object_bytes);
      if (!cbor_attestation_object || !cbor_attestation_object->is_map()) {
        FIDO_LOG(ERROR) << "invalid CBOR attestation object";
        return;
      }

      AttestationObject attestation_object;
      if (!device::cbor_extract::Extract<AttestationObject>(
              &attestation_object, kAttObjParseSteps,
              cbor_attestation_object->GetMap())) {
        FIDO_LOG(ERROR) << "attestation object parse failed";
        return;
      }

      cbor::Value::MapValue response_map;
      response_map.emplace(1, base::StringPiece(*attestation_object.fmt));
      response_map.emplace(
          2, base::span<const uint8_t>(*attestation_object.auth_data));
      response_map.emplace(3, attestation_object.statement->Clone());

      absl::optional<std::vector<uint8_t>> response_payload =
          cbor::Writer::Write(cbor::Value(std::move(response_map)));
      if (!response_payload) {
        return;
      }
      response.insert(response.end(), response_payload->begin(),
                      response_payload->end());
    } else {
      platform_->OnStatus(Platform::Status::CTAP_ERROR);
    }

    if (!transaction_done_) {
      platform_->OnStatus(Platform::Status::FIRST_TRANSACTION_DONE);
      transaction_done_ = true;
    }
    transport_->Write(std::move(response));
  }

  void OnGetAssertionResponse(
      uint32_t ctap_status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr auth_response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_LE(ctap_status, 0xFFu);

    if (auth_response && get_assertion_had_empty_allowlist_ &&
        !auth_response->user_handle) {
      FIDO_LOG(ERROR)
          << "missing user id in response to discoverable credential assertion";
      ctap_status =
          static_cast<uint32_t>(CtapDeviceResponseCode::kCtap2ErrOther);
    }

    std::vector<uint8_t> response = {base::checked_cast<uint8_t>(ctap_status)};
    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      cbor::Value::MapValue credential_descriptor;
      credential_descriptor.emplace("type", device::kPublicKey);
      credential_descriptor.emplace("id",
                                    std::move(auth_response->info->raw_id));
      cbor::Value::ArrayValue transports;
      transports.emplace_back("internal");
      transports.emplace_back("cable");
      credential_descriptor.emplace("transports", std::move(transports));
      cbor::Value::MapValue response_map;
      response_map.emplace(1, std::move(credential_descriptor));
      response_map.emplace(2,
                           std::move(auth_response->info->authenticator_data));
      response_map.emplace(3, std::move(auth_response->signature));

      if (get_assertion_had_empty_allowlist_) {
        cbor::Value::MapValue user_map;
        user_map.emplace("id", std::move(*auth_response->user_handle));
        // The `name` and `displayName` fields are not present in
        // `GetAssertionAuthenticatorResponse` because they aren't returned
        // at the WebAuthn level. CTAP 2.1 says that fields other than `id` are
        // only applicable "For multiple accounts per RP case, where the
        // authenticator does not have a display". But we assume that caBLE
        // devices do have a display and don't handle multiple GetAssertion
        // responses anyway.
        user_map.emplace("name", "");
        user_map.emplace("displayName", "");
        response_map.emplace(4, std::move(user_map));

        // This is the `userSelected` field, which indicates that additional
        // confirmation of the account selection isn't needed.
        response_map.emplace(6, true);
      }

      absl::optional<std::vector<uint8_t>> response_payload =
          cbor::Writer::Write(cbor::Value(std::move(response_map)));
      if (!response_payload) {
        return;
      }
      response.insert(response.end(), response_payload->begin(),
                      response_payload->end());
    } else {
      platform_->OnStatus(Platform::Status::CTAP_ERROR);
    }

    if (!transaction_done_) {
      platform_->OnStatus(Platform::Status::FIRST_TRANSACTION_DONE);
      transaction_done_ = true;
    }
    transport_->Write(std::move(response));
  }

  // CopyCredIds parses a series of `PublicKeyCredentialDescriptor`s from `in`
  // and appends them to `out`, returning true on success or false on error.
  static bool CopyCredIds(const cbor::Value::ArrayValue* in,
                          std::vector<PublicKeyCredentialDescriptor>* out) {
    if (!in) {
      return true;
    }

    return device::cbor_extract::ForEachPublicKeyEntry(
        *in, cbor::Value("id"),
        base::BindRepeating(
            [](std::vector<PublicKeyCredentialDescriptor>* out,
               const cbor::Value& value) -> bool {
              if (!value.is_bytestring()) {
                return false;
              }
              out->emplace_back(device::CredentialType::kPublicKey,
                                value.GetBytestring());
              return true;
            },
            base::Unretained(out)));
  }

  bool transaction_received_ = false;
  bool transaction_done_ = false;
  bool get_assertion_had_empty_allowlist_ = false;
  const std::unique_ptr<Transport> transport_;
  const std::unique_ptr<Platform> platform_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CTAP2Processor> weak_factory_{this};
};

static std::array<uint8_t, 32> DerivePairedSecret(
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const absl::optional<base::span<const uint8_t>>& contact_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id) {
  base::span<const uint8_t, kRootSecretSize> secret = root_secret;

  std::array<uint8_t, kRootSecretSize> per_contact_id_secret;
  if (contact_id) {
    // The root secret is not used directly to derive the paired secret because
    // we want the keys to change after an unlink. Unlinking invalidates and
    // replaces the contact ID therefore we derive paired secrets in two steps:
    // first using the contact ID to derive a secret from the root secret, and
    // then using the pairing ID to generate a secret from that.
    per_contact_id_secret =
        device::cablev2::Derive<EXTENT(per_contact_id_secret)>(
            root_secret, *contact_id,
            device::cablev2::DerivedValueType::kPerContactIDSecret);
    secret = per_contact_id_secret;
  }

  std::array<uint8_t, 32> paired_secret;
  paired_secret = device::cablev2::Derive<EXTENT(paired_secret)>(
      secret, pairing_id, device::cablev2::DerivedValueType::kPairedSecret);

  return paired_secret;
}

class PairingDataGenerator {
 public:
  static GeneratePairingDataCallback GetClosure(
      base::span<const uint8_t, kRootSecretSize> root_secret,
      const std::string& name,
      absl::optional<std::vector<uint8_t>> contact_id) {
    auto* generator =
        new PairingDataGenerator(root_secret, name, std::move(contact_id));
    return base::BindOnce(&PairingDataGenerator::Generate,
                          base::Owned(generator));
  }

 private:
  PairingDataGenerator(base::span<const uint8_t, kRootSecretSize> root_secret,
                       const std::string& name,
                       absl::optional<std::vector<uint8_t>> contact_id)
      : root_secret_(fido_parsing_utils::Materialize(root_secret)),
        name_(name),
        contact_id_(std::move(contact_id)) {}

  absl::optional<cbor::Value> Generate(
      base::span<const uint8_t, device::kP256X962Length> peer_public_key_x962,
      device::cablev2::HandshakeHash handshake_hash) {
    if (!contact_id_) {
      return absl::nullopt;
    }

    std::array<uint8_t, kPairingIDSize> pairing_id;
    crypto::RandBytes(pairing_id);
    const std::array<uint8_t, 32> paired_secret =
        DerivePairedSecret(root_secret_, *contact_id_, pairing_id);

    cbor::Value::MapValue map;
    map.emplace(1, std::move(*contact_id_));
    map.emplace(2, pairing_id);
    map.emplace(3, paired_secret);

    bssl::UniquePtr<EC_KEY> identity_key(IdentityKey(root_secret_));
    device::CableAuthenticatorIdentityKey public_key;
    CHECK_EQ(
        public_key.size(),
        EC_POINT_point2oct(EC_KEY_get0_group(identity_key.get()),
                           EC_KEY_get0_public_key(identity_key.get()),
                           POINT_CONVERSION_UNCOMPRESSED, public_key.data(),
                           public_key.size(), /*ctx=*/nullptr));

    map.emplace(4, public_key);
    map.emplace(5, name_);

    map.emplace(6,
                device::cablev2::CalculatePairingSignature(
                    identity_key.get(), peer_public_key_x962, handshake_hash));

    return cbor::Value(std::move(map));
  }

  const std::array<uint8_t, kRootSecretSize> root_secret_;
  const std::string name_;
  absl::optional<std::vector<uint8_t>> contact_id_;
};

}  // namespace

Platform::BLEAdvert::~BLEAdvert() = default;
Platform::~Platform() = default;
Transport::~Transport() = default;
Transaction::~Transaction() = default;

std::unique_ptr<Transaction> TransactWithPlaintextTransport(
    std::unique_ptr<Platform> platform,
    std::unique_ptr<Transport> transport) {
  return std::make_unique<CTAP2Processor>(std::move(transport),
                                          std::move(platform));
}

std::unique_ptr<Transaction> TransactFromQRCode(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    absl::optional<std::vector<uint8_t>> contact_id,
    bool use_new_crypter_construction) {
  auto generate_pairing_data = PairingDataGenerator::GetClosure(
      root_secret, authenticator_name, std::move(contact_id));

  Platform* const platform_ptr = platform.get();
  return std::make_unique<CTAP2Processor>(
      std::make_unique<TunnelTransport>(
          platform_ptr, network_context, qr_secret, peer_identity,
          use_new_crypter_construction, std::move(generate_pairing_data)),
      std::move(platform));
}

std::unique_ptr<Transaction> TransactFromFCM(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce,
    absl::optional<base::span<const uint8_t>> contact_id) {
  const std::array<uint8_t, 32> paired_secret =
      DerivePairedSecret(root_secret, contact_id, pairing_id);

  Platform* const platform_ptr = platform.get();
  return std::make_unique<CTAP2Processor>(
      std::make_unique<TunnelTransport>(platform_ptr, network_context,
                                        paired_secret, client_nonce, routing_id,
                                        tunnel_id, IdentityKey(root_secret)),
      std::move(platform));
}

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device
