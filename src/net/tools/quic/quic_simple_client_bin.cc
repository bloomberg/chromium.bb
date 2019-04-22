// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.
// Connects to a host using QUIC, sends a request to the provided URL, and
// displays the response.
//
// Some usage examples:
//
// Standard request/response:
//   quic_client http://www.google.com
//   quic_client http://www.google.com --quiet
//   quic_client https://www.google.com --port=443
//
// Use a specific version:
//   quic_client http://www.google.com --quic_version=23
//
// Send a POST instead of a GET:
//   quic_client http://www.google.com --body="this is a POST body"
//
// Append additional headers to the request:
//   quic_client http://www.google.com  --host=${IP}
//               --headers="Header-A: 1234; Header-B: 5678"
//
// Connect to a host different to the URL being requested:
//   quic_client mail.google.com --host=www.google.com
//
// Connect to a specific IP:
//   IP=`dig www.google.com +short | head -1`
//   quic_client www.google.com --host=${IP}
//
// Try to connect to a host which does not speak QUIC:
//   quic_client http://www.example.com

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/task/thread_pool/thread_pool.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/tools/quic/quic_simple_client.h"
#include "net/tools/quic/synchronous_host_resolver.h"
#include "url/gurl.h"

using net::CertVerifier;
using net::CTVerifier;
using net::MultiLogCTVerifier;
using quic::ProofVerifier;
using net::ProofVerifierChromium;
using quic::QuicStringPiece;
using quic::QuicTextUtils;
using net::TransportSecurityState;
using spdy::SpdyHeaderBlock;
using std::cout;
using std::cerr;
using std::endl;

// The IP or hostname the quic client will connect to.
std::string FLAGS_host = "";
// The port to connect to.
int32_t FLAGS_port = 0;
// If set, send a POST with this body.
std::string FLAGS_body = "";
// If set, contents are converted from hex to ascii, before sending as body of
// a POST. e.g. --body_hex=\"68656c6c6f\"
std::string FLAGS_body_hex = "";
// A semicolon separated list of key:value pairs to add to request headers.
std::string FLAGS_headers = "";
// Set to true for a quieter output experience.
bool FLAGS_quiet = false;
// QUIC version to speak, e.g. "21". If not set, then all available versions are
// offered in the handshake. The version label (e.g. "T099") can also be used.
std::string FLAGS_quic_version = "";
// QUIC IETF draft number to use over the wire.
int32_t FLAGS_quic_ietf_draft = 0;
// If true, a version mismatch in the handshake is not considered a failure.
// Useful for probing a server to determine if it speaks any version of QUIC.
bool FLAGS_version_mismatch_ok = false;
// If true, an HTTP response code of 3xx is considered to be a successful
// response, otherwise a failure.
bool FLAGS_redirect_is_success = true;
// Initial MTU of the connection.
int32_t FLAGS_initial_mtu = 0;

class FakeProofVerifier : public quic::ProofVerifier {
 public:
  quic::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      quic::QuicTransportVersion quic_version,
      quic::QuicStringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const quic::ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_SUCCESS;
  }

  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& hostname,
      const std::vector<std::string>& certs,
      const quic::ProofVerifyContext* verify_context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_SUCCESS;
  }

  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& urls = line->GetArgs();
  base::ThreadPool::CreateAndStartWithDefaultParams("quic_client");

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  if (line->HasSwitch("h") || line->HasSwitch("help") || urls.empty()) {
    const char* help_str =
        "Usage: quic_client [options] <url>\n"
        "\n"
        "<url> with scheme must be provided (e.g. http://www.google.com)\n\n"
        "Options:\n"
        "-h, --help                  show this help message and exit\n"
        "--host=<host>               specify the IP address of the hostname to "
        "connect to\n"
        "--port=<port>               specify the port to connect to\n"
        "--body=<body>               specify the body to post\n"
        "--body_hex=<body_hex>       specify the body_hex to be printed out\n"
        "--headers=<headers>         specify a semicolon separated list of "
        "key:value pairs to add to request headers\n"
        "--quiet                     specify for a quieter output experience\n"
        "--quic_version=<quic version> specify QUIC version to speak "
        "(e.g. 43, Q046 or T099).\n"
        "--quic_ietf_draft=<draft> specify which QUIC IETF draft number to use "
        "over the wire, e.g. 18. By default this sets quic_version to T099. "
        "This also enables required internal QUIC flags.\n"
        "--version_mismatch_ok       if specified a version mismatch in the "
        "handshake is not considered a failure\n"
        "--redirect_is_success       if specified an HTTP response code of 3xx "
        "is considered to be a successful response, otherwise a failure\n"
        "--initial_mtu=<initial_mtu> specify the initial MTU of the connection"
        "\n"
        "--disable_certificate_verification do not verify certificates\n";
    cout << help_str;
    exit(0);
  }
  if (line->HasSwitch("host")) {
    FLAGS_host = line->GetSwitchValueASCII("host");
  }
  if (line->HasSwitch("port")) {
    if (!base::StringToInt(line->GetSwitchValueASCII("port"), &FLAGS_port)) {
      std::cerr << "--port must be an integer\n";
      return 1;
    }
  }
  if (line->HasSwitch("body")) {
    FLAGS_body = line->GetSwitchValueASCII("body");
  }
  if (line->HasSwitch("body_hex")) {
    FLAGS_body_hex = line->GetSwitchValueASCII("body_hex");
  }
  if (line->HasSwitch("headers")) {
    FLAGS_headers = line->GetSwitchValueASCII("headers");
  }
  if (line->HasSwitch("quiet")) {
    FLAGS_quiet = true;
  }
  if (line->HasSwitch("quic_version")) {
    FLAGS_quic_version = line->GetSwitchValueASCII("quic_version");
  }
  if (line->HasSwitch("quic_ietf_draft")) {
    if (!base::StringToInt(line->GetSwitchValueASCII("quic_ietf_draft"),
                           &FLAGS_quic_ietf_draft)) {
      std::cerr << "--quic_ietf_draft must be an integer\n";
      return 1;
    }
  }
  if (line->HasSwitch("version_mismatch_ok")) {
    FLAGS_version_mismatch_ok = true;
  }
  if (line->HasSwitch("redirect_is_success")) {
    FLAGS_redirect_is_success = true;
  }
  if (line->HasSwitch("initial_mtu")) {
    if (!base::StringToInt(line->GetSwitchValueASCII("initial_mtu"),
                           &FLAGS_initial_mtu)) {
      std::cerr << "--initial_mtu must be an integer\n";
      return 1;
    }
  }

  VLOG(1) << "server host: " << FLAGS_host << " port: " << FLAGS_port
          << " body: " << FLAGS_body << " headers: " << FLAGS_headers
          << " quiet: " << FLAGS_quiet
          << " quic_version: " << FLAGS_quic_version
          << " quic_ietf_draft: " << FLAGS_quic_ietf_draft
          << " version_mismatch_ok: " << FLAGS_version_mismatch_ok
          << " redirect_is_success: " << FLAGS_redirect_is_success
          << " initial_mtu: " << FLAGS_initial_mtu;

  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  // Determine IP address to connect to from supplied hostname.
  quic::QuicIpAddress ip_addr;

  GURL url(urls[0]);
  std::string host = FLAGS_host;
  if (host.empty()) {
    host = url.host();
  }
  int port = FLAGS_port;
  if (port == 0) {
    port = url.EffectiveIntPort();
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

  std::string host_port = quic::QuicStrCat(ip_addr.ToString(), ":", port);
  VLOG(1) << "Resolved " << host << " to " << host_port << endl;

  // Build the client, and try to connect.
  quic::QuicServerId server_id(url.host(), url.EffectiveIntPort(),
                               net::PRIVACY_MODE_DISABLED);

  // Select QUIC version to use.
  quic::ParsedQuicVersionVector versions = quic::CurrentSupportedVersions();
  std::string quic_version_string = FLAGS_quic_version;
  if (FLAGS_quic_ietf_draft > 0) {
    quic::QuicVersionInitializeSupportForIetfDraft(FLAGS_quic_ietf_draft);
    if (quic_version_string.empty()) {
      quic_version_string = "T099";
    }
  }
  if (!quic_version_string.empty()) {
    if (quic_version_string[0] == 'T') {
      // ParseQuicVersionString checks quic_supports_tls_handshake.
      SetQuicFlag(&FLAGS_quic_supports_tls_handshake, true);
    }
    quic::ParsedQuicVersion parsed_quic_version =
        quic::ParseQuicVersionString(quic_version_string);
    if (parsed_quic_version.transport_version ==
        quic::QUIC_VERSION_UNSUPPORTED) {
      return 1;
    }
    versions.clear();
    versions.push_back(parsed_quic_version);
    quic::QuicEnableVersion(parsed_quic_version);
  }

  // For secure QUIC we need to verify the cert chain.
  std::unique_ptr<CertVerifier> cert_verifier(CertVerifier::CreateDefault());
  std::unique_ptr<TransportSecurityState> transport_security_state(
      new TransportSecurityState);
  std::unique_ptr<MultiLogCTVerifier> ct_verifier(new MultiLogCTVerifier());
  std::unique_ptr<net::CTPolicyEnforcer> ct_policy_enforcer(
      new net::DefaultCTPolicyEnforcer());
  std::unique_ptr<quic::ProofVerifier> proof_verifier;
  if (line->HasSwitch("disable_certificate_verification")) {
    proof_verifier.reset(new FakeProofVerifier());
  } else {
    proof_verifier.reset(new ProofVerifierChromium(
        cert_verifier.get(), ct_policy_enforcer.get(),
        transport_security_state.get(), ct_verifier.get()));
  }
  net::QuicSimpleClient client(quic::QuicSocketAddress(ip_addr, port),
                               server_id, versions, std::move(proof_verifier));
  client.set_initial_max_packet_length(
      FLAGS_initial_mtu != 0 ? FLAGS_initial_mtu : quic::kDefaultMaxPacketSize);
  if (!client.Initialize()) {
    cerr << "Failed to initialize client." << endl;
    return 1;
  }
  if (!client.Connect()) {
    quic::QuicErrorCode error = client.session()->error();
    if (FLAGS_version_mismatch_ok && error == quic::QUIC_INVALID_VERSION) {
      cout << "Server talks QUIC, but none of the versions supported by "
           << "this client: " << ParsedQuicVersionVectorToString(versions)
           << endl;
      // Version mismatch is not deemed a failure.
      return 0;
    }
    cerr << "Failed to connect to " << host_port
         << ". Error: " << quic::QuicErrorCodeToString(error) << endl;
    return 1;
  }
  cout << "Connected to " << host_port << endl;

  // Construct the string body from flags, if provided.
  std::string body = FLAGS_body;
  if (!FLAGS_body_hex.empty()) {
    DCHECK(FLAGS_body.empty()) << "Only set one of --body and --body_hex.";
    body = quic::QuicTextUtils::HexDecode(FLAGS_body_hex);
  }

  // Construct a GET or POST request for supplied URL.
  SpdyHeaderBlock header_block;
  header_block[":method"] = body.empty() ? "GET" : "POST";
  header_block[":scheme"] = url.scheme();
  header_block[":authority"] = url.host();
  header_block[":path"] = url.path();

  // Append any additional headers supplied on the command line.
  for (quic::QuicStringPiece sp :
       quic::QuicTextUtils::Split(FLAGS_headers, ';')) {
    quic::QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&sp);
    if (sp.empty()) {
      continue;
    }
    std::vector<quic::QuicStringPiece> kv = quic::QuicTextUtils::Split(sp, ':');
    quic::QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[0]);
    quic::QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[1]);
    header_block[kv[0]] = kv[1];
  }

  // Make sure to store the response, for later output.
  client.set_store_response(true);

  // Send the request.
  client.SendRequestAndWaitForResponse(header_block, body, /*fin=*/true);

  // Print request and response details.
  if (!FLAGS_quiet) {
    cout << "Request:" << endl;
    cout << "headers:" << header_block.DebugString();
    if (!FLAGS_body_hex.empty()) {
      // Print the user provided hex, rather than binary body.
      cout << "body:\n"
           << quic::QuicTextUtils::HexDump(
                  quic::QuicTextUtils::HexDecode(FLAGS_body_hex))
           << endl;
    } else {
      cout << "body: " << body << endl;
    }
    cout << endl;
    cout << "Response:" << endl;
    cout << "headers: " << client.latest_response_headers() << endl;
    std::string response_body = client.latest_response_body();
    if (!FLAGS_body_hex.empty()) {
      // Assume response is binary data.
      cout << "body:\n" << quic::QuicTextUtils::HexDump(response_body) << endl;
    } else {
      cout << "body: " << response_body << endl;
    }
    cout << "trailers: " << client.latest_response_trailers() << endl;
  }

  size_t response_code = client.latest_response_code();
  if (response_code >= 200 && response_code < 300) {
    cout << "Request succeeded (" << response_code << ")." << endl;
    return 0;
  } else if (response_code >= 300 && response_code < 400) {
    if (FLAGS_redirect_is_success) {
      cout << "Request succeeded (redirect " << response_code << ")." << endl;
      return 0;
    } else {
      cout << "Request failed (redirect " << response_code << ")." << endl;
      return 1;
    }
  } else {
    cerr << "Request failed (" << response_code << ")." << endl;
    return 1;
  }
}
