// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/masque/masque_client_session.h"

#include <cstring>
#include <string>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "url/url_canon.h"
#include "quiche/quic/core/http/spdy_utils.h"
#include "quiche/quic/core/quic_data_reader.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/tools/quic_url.h"
#include "quiche/common/platform/api/quiche_url_utils.h"

namespace quic {

MasqueClientSession::MasqueClientSession(
    MasqueMode masque_mode, const std::string& uri_template,
    const QuicConfig& config, const ParsedQuicVersionVector& supported_versions,
    QuicConnection* connection, const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config,
    QuicClientPushPromiseIndex* push_promise_index, Owner* owner)
    : QuicSpdyClientSession(config, supported_versions, connection, server_id,
                            crypto_config, push_promise_index),
      masque_mode_(masque_mode),
      uri_template_(uri_template),
      owner_(owner) {}

void MasqueClientSession::OnMessageAcked(QuicMessageId message_id,
                                         QuicTime /*receive_timestamp*/) {
  QUIC_DVLOG(1) << "Received ack for DATAGRAM frame " << message_id;
}

void MasqueClientSession::OnMessageLost(QuicMessageId message_id) {
  QUIC_DVLOG(1) << "We believe DATAGRAM frame " << message_id << " was lost";
}

const MasqueClientSession::ConnectUdpClientState*
MasqueClientSession::GetOrCreateConnectUdpClientState(
    const QuicSocketAddress& target_server_address,
    EncapsulatedClientSession* encapsulated_client_session) {
  for (const ConnectUdpClientState& client_state : connect_udp_client_states_) {
    if (client_state.target_server_address() == target_server_address &&
        client_state.encapsulated_client_session() ==
            encapsulated_client_session) {
      // Found existing CONNECT-UDP request.
      return &client_state;
    }
  }
  // No CONNECT-UDP request found, create a new one.

  url::Parsed parsed_uri_template;
  url::ParseStandardURL(uri_template_.c_str(), uri_template_.length(),
                        &parsed_uri_template);
  if (!parsed_uri_template.path.is_nonempty()) {
    QUIC_BUG(bad URI template path)
        << "Cannot parse path from URI template " << uri_template_;
    return nullptr;
  }
  std::string path = uri_template_.substr(parsed_uri_template.path.begin,
                                          parsed_uri_template.path.len);
  if (parsed_uri_template.query.is_valid()) {
    absl::StrAppend(&path, "?",
                    uri_template_.substr(parsed_uri_template.query.begin,
                                         parsed_uri_template.query.len));
  }
  absl::flat_hash_map<std::string, std::string> parameters;
  parameters["target_host"] = target_server_address.host().ToString();
  parameters["target_port"] = absl::StrCat(target_server_address.port());
  std::string expanded_path;
  absl::flat_hash_set<std::string> vars_found;
  bool expanded =
      quiche::ExpandURITemplate(path, parameters, &expanded_path, &vars_found);
  if (!expanded || vars_found.find("target_host") == vars_found.end() ||
      vars_found.find("target_port") == vars_found.end()) {
    QUIC_DLOG(ERROR) << "Failed to expand URI template \"" << uri_template_
                     << "\" for " << target_server_address;
    return nullptr;
  }

  url::Component expanded_path_component(0, expanded_path.length());
  url::RawCanonOutput<1024> canonicalized_path_output;
  url::Component canonicalized_path_component;
  bool canonicalized = url::CanonicalizePath(
      expanded_path.c_str(), expanded_path_component,
      &canonicalized_path_output, &canonicalized_path_component);
  if (!canonicalized || !canonicalized_path_component.is_nonempty()) {
    QUIC_DLOG(ERROR) << "Failed to canonicalize URI template \""
                     << uri_template_ << "\" for " << target_server_address;
    return nullptr;
  }
  std::string canonicalized_path(
      canonicalized_path_output.data() + canonicalized_path_component.begin,
      canonicalized_path_component.len);

  QuicSpdyClientStream* stream = CreateOutgoingBidirectionalStream();
  if (stream == nullptr) {
    // Stream flow control limits prevented us from opening a new stream.
    QUIC_DLOG(ERROR) << "Failed to open CONNECT-UDP stream";
    return nullptr;
  }

  QuicUrl url(uri_template_);
  std::string scheme = url.scheme();
  std::string authority = url.HostPort();

  QUIC_DLOG(INFO) << "Sending CONNECT-UDP request for " << target_server_address
                  << " on stream " << stream->id() << " scheme=\"" << scheme
                  << "\" authority=\"" << authority << "\" path=\""
                  << canonicalized_path << "\"";

  // Send the request.
  spdy::Http2HeaderBlock headers;
  headers[":method"] = "CONNECT";
  headers[":protocol"] = "connect-udp";
  headers[":scheme"] = scheme;
  headers[":authority"] = authority;
  headers[":path"] = canonicalized_path;
  headers["connect-udp-version"] = "12";
  size_t bytes_sent =
      stream->SendRequest(std::move(headers), /*body=*/"", /*fin=*/false);
  if (bytes_sent == 0) {
    QUIC_DLOG(ERROR) << "Failed to send CONNECT-UDP request";
    return nullptr;
  }

  connect_udp_client_states_.push_back(ConnectUdpClientState(
      stream, encapsulated_client_session, this, target_server_address));
  return &connect_udp_client_states_.back();
}

void MasqueClientSession::SendPacket(
    absl::string_view packet, const QuicSocketAddress& target_server_address,
    EncapsulatedClientSession* encapsulated_client_session) {
  const ConnectUdpClientState* connect_udp = GetOrCreateConnectUdpClientState(
      target_server_address, encapsulated_client_session);
  if (connect_udp == nullptr) {
    QUIC_DLOG(ERROR) << "Failed to create CONNECT-UDP request";
    return;
  }

  std::string http_payload;
  http_payload.resize(1 + packet.size());
  http_payload[0] = 0;
  memcpy(&http_payload[1], packet.data(), packet.size());
  MessageStatus message_status =
      SendHttp3Datagram(connect_udp->stream()->id(), http_payload);

  QUIC_DVLOG(1) << "Sent packet to " << target_server_address
                << " compressed with stream ID " << connect_udp->stream()->id()
                << " and got message status "
                << MessageStatusToString(message_status);
}

void MasqueClientSession::CloseConnectUdpStream(
    EncapsulatedClientSession* encapsulated_client_session) {
  for (auto it = connect_udp_client_states_.begin();
       it != connect_udp_client_states_.end();) {
    if (it->encapsulated_client_session() == encapsulated_client_session) {
      QUIC_DLOG(INFO) << "Removing state for stream ID " << it->stream()->id();
      auto* stream = it->stream();
      it = connect_udp_client_states_.erase(it);
      if (!stream->write_side_closed()) {
        stream->Reset(QUIC_STREAM_CANCELLED);
      }
    } else {
      ++it;
    }
  }
}

void MasqueClientSession::OnConnectionClosed(
    const QuicConnectionCloseFrame& frame, ConnectionCloseSource source) {
  QuicSpdyClientSession::OnConnectionClosed(frame, source);
  // Close all encapsulated sessions.
  for (const auto& client_state : connect_udp_client_states_) {
    client_state.encapsulated_client_session()->CloseConnection(
        QUIC_CONNECTION_CANCELLED, "Underlying MASQUE connection was closed",
        ConnectionCloseBehavior::SILENT_CLOSE);
  }
}

void MasqueClientSession::OnStreamClosed(QuicStreamId stream_id) {
  if (QuicUtils::IsBidirectionalStreamId(stream_id, version()) &&
      QuicUtils::IsClientInitiatedStreamId(transport_version(), stream_id)) {
    QuicSpdyClientStream* stream =
        reinterpret_cast<QuicSpdyClientStream*>(GetActiveStream(stream_id));
    if (stream != nullptr) {
      QUIC_DLOG(INFO) << "Stream " << stream_id
                      << " closed, got response headers:"
                      << stream->response_headers().DebugString();
    }
  }
  for (auto it = connect_udp_client_states_.begin();
       it != connect_udp_client_states_.end();) {
    if (it->stream()->id() == stream_id) {
      QUIC_DLOG(INFO) << "Stream " << stream_id
                      << " was closed, removing state";
      auto* encapsulated_client_session = it->encapsulated_client_session();
      it = connect_udp_client_states_.erase(it);
      encapsulated_client_session->CloseConnection(
          QUIC_CONNECTION_CANCELLED,
          "Underlying MASQUE CONNECT-UDP stream was closed",
          ConnectionCloseBehavior::SILENT_CLOSE);
    } else {
      ++it;
    }
  }

  QuicSpdyClientSession::OnStreamClosed(stream_id);
}

bool MasqueClientSession::OnSettingsFrame(const SettingsFrame& frame) {
  QUIC_DLOG(INFO) << "Received SETTINGS: " << frame;
  if (!QuicSpdyClientSession::OnSettingsFrame(frame)) {
    QUIC_DLOG(ERROR) << "Failed to parse received settings";
    return false;
  }
  if (!SupportsH3Datagram()) {
    QUIC_DLOG(ERROR) << "Refusing to use MASQUE without HTTP/3 Datagrams";
    return false;
  }
  QUIC_DLOG(INFO) << "Using HTTP Datagram: " << http_datagram_support();
  owner_->OnSettingsReceived();
  return true;
}

MasqueClientSession::ConnectUdpClientState::ConnectUdpClientState(
    QuicSpdyClientStream* stream,
    EncapsulatedClientSession* encapsulated_client_session,
    MasqueClientSession* masque_session,
    const QuicSocketAddress& target_server_address)
    : stream_(stream),
      encapsulated_client_session_(encapsulated_client_session),
      masque_session_(masque_session),
      target_server_address_(target_server_address) {
  QUICHE_DCHECK_NE(masque_session_, nullptr);
  this->stream()->RegisterHttp3DatagramVisitor(this);
}

MasqueClientSession::ConnectUdpClientState::~ConnectUdpClientState() {
  if (stream() != nullptr) {
    stream()->UnregisterHttp3DatagramVisitor();
  }
}

MasqueClientSession::ConnectUdpClientState::ConnectUdpClientState(
    MasqueClientSession::ConnectUdpClientState&& other) {
  *this = std::move(other);
}

MasqueClientSession::ConnectUdpClientState&
MasqueClientSession::ConnectUdpClientState::operator=(
    MasqueClientSession::ConnectUdpClientState&& other) {
  stream_ = other.stream_;
  encapsulated_client_session_ = other.encapsulated_client_session_;
  masque_session_ = other.masque_session_;
  target_server_address_ = other.target_server_address_;
  other.stream_ = nullptr;
  if (stream() != nullptr) {
    stream()->ReplaceHttp3DatagramVisitor(this);
  }
  return *this;
}

void MasqueClientSession::ConnectUdpClientState::OnHttp3Datagram(
    QuicStreamId stream_id, absl::string_view payload) {
  QUICHE_DCHECK_EQ(stream_id, stream()->id());
  QuicDataReader reader(payload);
  uint64_t context_id;
  if (!reader.ReadVarInt62(&context_id)) {
    QUIC_DLOG(ERROR) << "Failed to read context ID";
    return;
  }
  if (context_id != 0) {
    QUIC_DLOG(ERROR) << "Ignoring HTTP Datagram with unexpected context ID "
                     << context_id;
    return;
  }
  absl::string_view http_payload = reader.ReadRemainingPayload();
  encapsulated_client_session_->ProcessPacket(http_payload,
                                              target_server_address_);
  QUIC_DVLOG(1) << "Sent " << http_payload.size()
                << " bytes to connection for stream ID " << stream_id;
}

}  // namespace quic
