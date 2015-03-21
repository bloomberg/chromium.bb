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
//   quic_client http://www.google.com --version=23  --host=${IP}
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
// Try to connect to a host which does not speak QUIC:
//   Get IP address of the www.example.com
//   IP=`dig www.example.com +short | head -1`
//   quic_client http://www.example.com --host=${IP}

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/privacy_mode.h"
#include "net/cert/cert_verifier.h"
#include "net/http/http_request_info.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/quic_utils.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/tools/quic/quic_simple_client.h"
#include "net/tools/quic/synchronous_host_resolver.h"
#include "url/gurl.h"

using base::StringPiece;
using net::CertVerifier;
using net::ProofVerifierChromium;
using net::TransportSecurityState;
using std::cout;
using std::cerr;
using std::map;
using std::string;
using std::vector;
using std::endl;

// The IP or hostname the quic client will connect to.
string FLAGS_host = "";
// The port to connect to.
int32 FLAGS_port = 80;
// If set, send a POST with this body.
string FLAGS_body = "";
// A semicolon separated list of key:value pairs to add to request headers.
string FLAGS_headers = "";
// Set to true for a quieter output experience.
bool FLAGS_quiet = false;
// QUIC version to speak, e.g. 21. If not set, then all available versions are
// offered in the handshake.
int32 FLAGS_quic_version = -1;
// If true, a version mismatch in the handshake is not considered a failure.
// Useful for probing a server to determine if it speaks any version of QUIC.
bool FLAGS_version_mismatch_ok = false;
// If true, an HTTP response code of 3xx is considered to be a successful
// response, otherwise a failure.
bool FLAGS_redirect_is_success = true;

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& urls = line->GetArgs();

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
        "--headers=<headers>         specify a semicolon separated list of "
        "key:value pairs to add to request headers\n"
        "--quiet                     specify for a quieter output experience\n"
        "--quic-version=<quic version> specify QUIC version to speak\n"
        "--version_mismatch_ok       if specified a version mismatch in the "
        "handshake is not considered a failure\n"
        "--redirect_is_success       if specified an HTTP response code of 3xx "
        "is considered to be a successful response, otherwise a failure\n";
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
  if (line->HasSwitch("headers")) {
    FLAGS_headers = line->GetSwitchValueASCII("headers");
  }
  if (line->HasSwitch("quiet")) {
    FLAGS_quiet = true;
  }
  if (line->HasSwitch("quic-version")) {
    int quic_version;
    if (base::StringToInt(line->GetSwitchValueASCII("quic-version"),
                          &quic_version)) {
      FLAGS_quic_version = quic_version;
    }
  }
  if (line->HasSwitch("version_mismatch_ok")) {
    FLAGS_version_mismatch_ok = true;
  }
  if (line->HasSwitch("redirect_is_success")) {
    FLAGS_redirect_is_success = true;
  }

  VLOG(1) << "server host: " << FLAGS_host << " port: " << FLAGS_port
          << " body: " << FLAGS_body << " headers: " << FLAGS_headers
          << " quiet: " << FLAGS_quiet
          << " quic-version: " << FLAGS_quic_version
          << " version_mismatch_ok: " << FLAGS_version_mismatch_ok
          << " redirect_is_success: " << FLAGS_redirect_is_success;

  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  // Determine IP address to connect to from supplied hostname.
  net::IPAddressNumber ip_addr;

  // TODO(rtenneti): GURL's doesn't support default_protocol argument, thus
  // protocol is required in the URL.
  GURL url(urls[0]);
  string host = FLAGS_host;
  if (host.empty()) {
    host = url.host();
  }
  if (!net::ParseIPLiteralToNumber(host, &ip_addr)) {
    net::AddressList addresses;
    int rv = net::tools::SynchronousHostResolver::Resolve(host, &addresses);
    if (rv != net::OK) {
      LOG(ERROR) << "Unable to resolve '" << host << "' : "
                 << net::ErrorToString(rv);
      return 1;
    }
    ip_addr = addresses[0].address();
  }

  string host_port = net::IPAddressToStringWithPort(ip_addr, FLAGS_port);
  VLOG(1) << "Resolved " << host << " to " << host_port << endl;

  // Build the client, and try to connect.
  bool is_https = (FLAGS_port == 443);
  net::QuicServerId server_id(host, FLAGS_port, is_https,
                              net::PRIVACY_MODE_DISABLED);
  net::QuicVersionVector versions = net::QuicSupportedVersions();
  if (FLAGS_quic_version != -1) {
    versions.clear();
    versions.push_back(static_cast<net::QuicVersion>(FLAGS_quic_version));
  }
  net::tools::QuicSimpleClient client(net::IPEndPoint(ip_addr, FLAGS_port),
                                      server_id, versions);
  scoped_ptr<CertVerifier> cert_verifier;
  scoped_ptr<TransportSecurityState> transport_security_state;
  if (is_https) {
    // For secure QUIC we need to verify the cert chain.a
    cert_verifier.reset(CertVerifier::CreateDefault());
    transport_security_state.reset(new TransportSecurityState);
    // TODO(rtenneti): Fix "Proof invalid: Missing context" error.
    client.SetProofVerifier(new ProofVerifierChromium(
        cert_verifier.get(), transport_security_state.get()));
  }
  if (!client.Initialize()) {
    cerr << "Failed to initialize client." << endl;
    return 1;
  }
  if (!client.Connect()) {
    net::QuicErrorCode error = client.session()->error();
    if (FLAGS_version_mismatch_ok && error == net::QUIC_INVALID_VERSION) {
      cout << "Server talks QUIC, but none of the versions supoorted by "
           << "this client: " << QuicVersionVectorToString(versions) << endl;
      // Version mismatch is not deemed a failure.
      return 0;
    }
    cerr << "Failed to connect to " << host_port
         << ". Error: " << net::QuicUtils::ErrorToString(error) << endl;
    return 1;
  }
  cout << "Connected to " << host_port << endl;

  // Construct a GET or POST request for supplied URL.
  net::HttpRequestInfo request;
  request.method = FLAGS_body.empty() ? "GET" : "POST";
  request.url = url;

  // Append any additional headers supplied on the command line.
  vector<string> headers_tokenized;
  Tokenize(FLAGS_headers, ";", &headers_tokenized);
  for (size_t i = 0; i < headers_tokenized.size(); ++i) {
    string sp;
    base::TrimWhitespaceASCII(headers_tokenized[i], base::TRIM_ALL, &sp);
    if (sp.empty()) {
      continue;
    }
    vector<string> kv;
    base::SplitString(sp, ':', &kv);
    CHECK_EQ(2u, kv.size());
    string key;
    base::TrimWhitespaceASCII(kv[0], base::TRIM_ALL, &key);
    string value;
    base::TrimWhitespaceASCII(kv[1], base::TRIM_ALL, &value);
    request.extra_headers.SetHeader(key, value);
  }

  // Make sure to store the response, for later output.
  client.set_store_response(true);

  // Send the request.
  net::SpdyHeaderBlock header_block;
  net::CreateSpdyHeadersFromHttpRequest(request, request.extra_headers,
                                        net::SPDY3, /*direct=*/ true,
                                        &header_block);
  client.SendRequestAndWaitForResponse(request, FLAGS_body, /*fin=*/true);

  // Print request and response details.
  if (!FLAGS_quiet) {
    cout << "Request:" << endl;
    cout << "headers:" << endl;
    for (const std::pair<string, string>& kv : header_block) {
      cout << " " << kv.first << ": " << kv.second << endl;
    }
    cout << "body: " << FLAGS_body << endl;
    cout << endl;
    cout << "Response:" << endl;
    cout << "headers: " << client.latest_response_headers() << endl;
    cout << "body: " << client.latest_response_body() << endl;
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
