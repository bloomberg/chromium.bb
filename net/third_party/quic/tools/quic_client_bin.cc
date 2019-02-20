// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.
// Connects to a host using QUIC, sends a request to the provided URL, and
// displays the response.
//
// Some usage examples:
//
//   TODO(rtenneti): make --host optional by getting IP Address of URL's host.
//
//   Get IP address of the www.google.com
//   IP=`dig www.google.com +short | head -1`
//
// Standard request/response:
//   quic_client http://www.google.com  --host=${IP}
//   quic_client http://www.google.com --quiet  --host=${IP}
//   quic_client https://www.google.com --port=443  --host=${IP}
//
// Use a specific version:
//   quic_client http://www.google.com --quic_version=23  --host=${IP}
//
// Send a POST instead of a GET:
//   quic_client http://www.google.com --body="this is a POST body" --host=${IP}
//
// Append additional headers to the request:
//   quic_client http://www.google.com  --host=${IP}
//               --headers="Header-A: 1234; Header-B: 5678"
//
// Connect to a host different to the URL being requested:
//   Get IP address of the www.google.com
//   IP=`dig www.google.com +short | head -1`
//   quic_client mail.google.com --host=${IP}
//
// Send repeated requests and change ephemeral port between requests
//   quic_client www.google.com --num_requests=10
//
// Try to connect to a host which does not speak QUIC:
//   Get IP address of the www.example.com
//   IP=`dig www.example.com +short | head -1`
//   quic_client http://www.example.com --host=${IP}

#include <iostream>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/platform/api/quic_default_proof_providers.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/tools/quic_client.h"
#include "net/third_party/quic/tools/quic_url.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/synchronous_host_resolver.h"

using quic::ProofVerifier;
using quic::QuicString;
using quic::QuicStringPiece;
using quic::QuicTextUtils;
using quic::QuicUrl;
using spdy::SpdyHeaderBlock;
using std::cerr;
using std::cout;
using std::endl;
using std::string;

DEFINE_QUIC_COMMAND_LINE_FLAG(
    string,
    host,
    "",
    "The IP or hostname the quic client will connect to.");

DEFINE_QUIC_COMMAND_LINE_FLAG(int32_t, port, 0, "The port to connect to.");

DEFINE_QUIC_COMMAND_LINE_FLAG(string,
                              body,
                              "",
                              "If set, send a POST with this body.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    string,
    body_hex,
    "",
    "If set, contents are converted from hex to ascii, before sending as body "
    "of a POST. e.g. --body_hex=\"68656c6c6f\"");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    string,
    headers,
    "",
    "A semicolon separated list of key:value pairs to add to request headers.");

DEFINE_QUIC_COMMAND_LINE_FLAG(bool,
                              quiet,
                              false,
                              "Set to true for a quieter output experience.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    int32_t,
    quic_version,
    -1,
    "QUIC version to speak, e.g. 21. If not set, then all available versions "
    "are offered in the handshake.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    bool,
    version_mismatch_ok,
    false,
    "If true, a version mismatch in the handshake is not considered a failure. "
    "Useful for probing a server to determine if it speaks any version of "
    "QUIC.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    bool,
    redirect_is_success,
    true,
    "If true, an HTTP response code of 3xx is considered to be a successful "
    "response, otherwise a failure.");

DEFINE_QUIC_COMMAND_LINE_FLAG(int32_t,
                              initial_mtu,
                              0,
                              "Initial MTU of the connection.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    int32_t,
    num_requests,
    1,
    "How many sequential requests to make on a single connection.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    bool,
    drop_response_body,
    false,
    "If true, drop response body immediately after it is received.");

DEFINE_QUIC_COMMAND_LINE_FLAG(bool,
                              disable_certificate_verification,
                              false,
                              "If true, do not verify certificates.");

class FakeProofVerifier : public ProofVerifier {
 public:
  quic::QuicAsyncStatus VerifyProof(
      const string& /*hostname*/,
      const uint16_t /*port*/,
      const string& /*server_config*/,
      quic::QuicTransportVersion /*quic_version*/,
      QuicStringPiece /*chlo_hash*/,
      const std::vector<string>& /*certs*/,
      const string& /*cert_sct*/,
      const string& /*signature*/,
      const quic::ProofVerifyContext* /*context*/,
      string* /*error_details*/,
      std::unique_ptr<quic::ProofVerifyDetails>* /*details*/,
      std::unique_ptr<quic::ProofVerifierCallback> /*callback*/) override {
    return quic::QUIC_SUCCESS;
  }

  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& /*hostname*/,
      const std::vector<std::string>& /*certs*/,
      const quic::ProofVerifyContext* /*verify_context*/,
      std::string* /*error_details*/,
      std::unique_ptr<quic::ProofVerifyDetails>* /*verify_details*/,
      std::unique_ptr<quic::ProofVerifierCallback> /*callback*/) override {
    return quic::QUIC_SUCCESS;
  }
  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

int main(int argc, char* argv[]) {
  base::TaskScheduler::CreateAndStartWithDefaultParams("quic_client");
  const char* usage =
      "Usage: epoll_quic_client [options] <url>\n"
      "\n"
      "<url> with scheme must be provided (e.g. http://www.google.com)\n";
  std::vector<QuicString> urls =
      quic::QuicParseCommandLineFlags(usage, argc, argv);
  if (urls.empty()) {
    quic::QuicPrintCommandLineFlagHelp(usage);
    exit(0);
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  VLOG(1) << "server host: " << GetQuicFlag(FLAGS_host)
          << " port: " << GetQuicFlag(FLAGS_port)
          << " body: " << GetQuicFlag(FLAGS_body)
          << " headers: " << GetQuicFlag(FLAGS_headers)
          << " quiet: " << GetQuicFlag(FLAGS_quiet)
          << " quic-version: " << GetQuicFlag(FLAGS_quic_version)
          << " version_mismatch_ok: " << GetQuicFlag(FLAGS_version_mismatch_ok)
          << " redirect_is_success: " << GetQuicFlag(FLAGS_redirect_is_success)
          << " initial_mtu: " << GetQuicFlag(FLAGS_initial_mtu);

  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  // Determine IP address to connect to from supplied hostname.
  quic::QuicIpAddress ip_addr;

  QuicUrl url(urls[0], "https");
  string host = GetQuicFlag(FLAGS_host);
  if (host.empty()) {
    host = url.host();
  }
  int port = GetQuicFlag(FLAGS_port);
  if (port == 0) {
    port = url.port();
  }
  if (!ip_addr.FromString(host)) {
    net::AddressList addresses;
    int rv = net::SynchronousHostResolver::Resolve(host, &addresses);
    if (rv != net::OK) {
      LOG(ERROR) << "Unable to resolve '" << host
                 << "' : " << net::ErrorToShortString(rv);
      return 1;
    }
    ip_addr =
        quic::QuicIpAddress(quic::QuicIpAddressImpl(addresses[0].address()));
  }

  string host_port = quic::QuicStrCat(ip_addr.ToString(), ":", port);
  VLOG(1) << "Resolved " << host << " to " << host_port << endl;

  // Build the client, and try to connect.
  quic::QuicEpollServer epoll_server;
  quic::QuicServerId server_id(url.host(), port, false);
  quic::ParsedQuicVersionVector versions = quic::CurrentSupportedVersions();
  if (GetQuicFlag(FLAGS_quic_version) != -1) {
    versions.clear();
    versions.push_back(quic::ParsedQuicVersion(
        quic::PROTOCOL_QUIC_CRYPTO, static_cast<quic::QuicTransportVersion>(
                                        GetQuicFlag(FLAGS_quic_version))));
  }
  const int32_t num_requests = GetQuicFlag(FLAGS_num_requests);
  // For secure QUIC we need to verify the cert chain.
  std::unique_ptr<ProofVerifier> proof_verifier;
  if (GetQuicFlag(FLAGS_disable_certificate_verification)) {
    proof_verifier = quic::QuicMakeUnique<FakeProofVerifier>();
  } else {
    proof_verifier = quic::CreateDefaultProofVerifier();
  }
  quic::QuicClient client(quic::QuicSocketAddress(ip_addr, port), server_id,
                          versions, &epoll_server, std::move(proof_verifier));
  client.set_initial_max_packet_length(GetQuicFlag(FLAGS_initial_mtu) != 0
                                           ? GetQuicFlag(FLAGS_initial_mtu)
                                           : quic::kDefaultMaxPacketSize);
  client.set_drop_response_body(GetQuicFlag(FLAGS_drop_response_body));
  if (!client.Initialize()) {
    cerr << "Failed to initialize client." << endl;
    return 1;
  }
  if (!client.Connect()) {
    quic::QuicErrorCode error = client.session()->error();
    if (error == quic::QUIC_INVALID_VERSION) {
      cout << "Server talks QUIC, but none of the versions supported by "
           << "this client: " << ParsedQuicVersionVectorToString(versions)
           << endl;
      // 0: No error.
      // 20: Failed to connect due to QUIC_INVALID_VERSION.
      return GetQuicFlag(FLAGS_version_mismatch_ok) ? 0 : 20;
    }
    cerr << "Failed to connect to " << host_port
         << ". Error: " << quic::QuicErrorCodeToString(error) << endl;
    return 1;
  }
  cout << "Connected to " << host_port << endl;

  // Construct the string body from flags, if provided.
  string body = GetQuicFlag(FLAGS_body);
  if (!GetQuicFlag(FLAGS_body_hex).empty()) {
    DCHECK(GetQuicFlag(FLAGS_body).empty())
        << "Only set one of --body and --body_hex.";
    body = QuicTextUtils::HexDecode(GetQuicFlag(FLAGS_body_hex));
  }

  // Construct a GET or POST request for supplied URL.
  spdy::SpdyHeaderBlock header_block;
  header_block[":method"] = body.empty() ? "GET" : "POST";
  header_block[":scheme"] = url.scheme();
  header_block[":authority"] = url.HostPort();
  header_block[":path"] = url.PathParamsQuery();

  // Append any additional headers supplied on the command line.
  for (QuicStringPiece sp :
       QuicTextUtils::Split(GetQuicFlag(FLAGS_headers), ';')) {
    QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&sp);
    if (sp.empty()) {
      continue;
    }
    std::vector<QuicStringPiece> kv = QuicTextUtils::Split(sp, ':');
    QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[0]);
    QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[1]);
    header_block[kv[0]] = kv[1];
  }

  // Make sure to store the response, for later output.
  client.set_store_response(true);

  for (int i = 0; i < num_requests; ++i) {
    // Send the request.
    client.SendRequestAndWaitForResponse(header_block, body, /*fin=*/true);

    // Print request and response details.
    if (!GetQuicFlag(FLAGS_quiet)) {
      cout << "Request:" << endl;
      cout << "headers:" << header_block.DebugString();
      if (!GetQuicFlag(FLAGS_body_hex).empty()) {
        // Print the user provided hex, rather than binary body.
        cout << "body:\n"
             << QuicTextUtils::HexDump(
                    QuicTextUtils::HexDecode(GetQuicFlag(FLAGS_body_hex)))
             << endl;
      } else {
        cout << "body: " << body << endl;
      }
      cout << endl;

      if (!client.preliminary_response_headers().empty()) {
        cout << "Preliminary response headers: "
             << client.preliminary_response_headers() << endl;
        cout << endl;
      }

      cout << "Response:" << endl;
      cout << "headers: " << client.latest_response_headers() << endl;
      string response_body = client.latest_response_body();
      if (!GetQuicFlag(FLAGS_body_hex).empty()) {
        // Assume response is binary data.
        cout << "body:\n" << QuicTextUtils::HexDump(response_body) << endl;
      } else {
        cout << "body: " << response_body << endl;
      }
      cout << "trailers: " << client.latest_response_trailers() << endl;
    }

    if (!client.connected()) {
      cerr << "Request caused connection failure. Error: "
           << quic::QuicErrorCodeToString(client.session()->error()) << endl;
      return 1;
    }

    size_t response_code = client.latest_response_code();
    if (response_code >= 200 && response_code < 300) {
      cout << "Request succeeded (" << response_code << ")." << endl;
    } else if (response_code >= 300 && response_code < 400) {
      if (GetQuicFlag(FLAGS_redirect_is_success)) {
        cout << "Request succeeded (redirect " << response_code << ")." << endl;
      } else {
        cout << "Request failed (redirect " << response_code << ")." << endl;
        return 1;
      }
    } else {
      cerr << "Request failed (" << response_code << ")." << endl;
      return 1;
    }

    // Change the ephemeral port if there are more requests to do.
    if (i + 1 < num_requests) {
      if (!client.ChangeEphemeralPort()) {
        cout << "Failed to change ephemeral port." << endl;
        return 1;
      }
    }
  }

  return 0;
}
