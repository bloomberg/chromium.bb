// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/cert/ocsp_revocation_status.h"
#include "net/cert/x509_certificate.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/ssl/ssl_server_config.h"
#include "url/gurl.h"

namespace net {

class StreamSocket;
class TCPServerSocket;

namespace test_server {

class EmbeddedTestServerConnectionListener;
class EmbeddedTestServerHandle;
class HttpConnection;
class HttpResponse;
struct HttpRequest;

// Class providing an HTTP server for testing purpose. This is a basic server
// providing only an essential subset of HTTP/1.1 protocol. Especially,
// it assumes that the request syntax is correct. It *does not* support
// a Chunked Transfer Encoding.
//
// The common use case for unit tests is below:
//
// void SetUp() {
//   test_server_ = std::make_unique<EmbeddedTestServer>();
//   test_server_->RegisterRequestHandler(
//       base::BindRepeating(&FooTest::HandleRequest, base::Unretained(this)));
//   ASSERT_TRUE((test_server_handle_ = test_server_.StartAndReturnHandle()));
// }
//
// std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
//   GURL absolute_url = test_server_->GetURL(request.relative_url);
//   if (absolute_url.path() != "/test")
//     return std::unique_ptr<HttpResponse>();
//
//   auto http_response = std::make_unique<BasicHttpResponse>();
//   http_response->set_code(net::HTTP_OK);
//   http_response->set_content("hello");
//   http_response->set_content_type("text/plain");
//   return http_response;
// }
//
// For a test that spawns another process such as browser_tests, it is
// suggested to call Start in SetUpOnMainThread after the process is spawned.
//  If you have to do it before the process spawns, you need to first setup the
// listen socket so that there is no no other threads running while spawning
// the process. To do so, please follow the following example:
//
// void SetUp() {
//   ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
//   ...
//   InProcessBrowserTest::SetUp();
// }
//
// void SetUpOnMainThread() {
//   // Starts the accept IO thread.
//   embedded_test_server()->StartAcceptingConnections();
// }
//
class EmbeddedTestServer {
 public:
  enum Type {
    TYPE_HTTP,
    TYPE_HTTPS,
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net.test
  enum ServerCertificate {
    CERT_OK,

    CERT_MISMATCHED_NAME,
    CERT_EXPIRED,

    // Cross-signed certificate to test PKIX path building. Contains an
    // intermediate cross-signed by an unknown root, while the client (via
    // TestRootStore) is expected to have a self-signed version of the
    // intermediate.
    CERT_CHAIN_WRONG_ROOT,

    // Causes the testserver to use a hostname that is a domain
    // instead of an IP.
    CERT_COMMON_NAME_IS_DOMAIN,

    // A certificate that only contains a commonName, rather than also
    // including a subjectAltName extension.
    CERT_COMMON_NAME_ONLY,

    // A certificate that is a leaf certificate signed with SHA-1.
    CERT_SHA1_LEAF,

    // A certificate that is signed by an intermediate certificate.
    CERT_OK_BY_INTERMEDIATE,

    // A certificate with invalid notBefore and notAfter times. Windows'
    // certificate library will not parse this certificate.
    CERT_BAD_VALIDITY,

    // A certificate that covers a number of test names. See [test_names] in
    // net/data/ssl/scripts/ee.cnf. More may be added by editing this list and
    // and rerunning net/data/ssl/scripts/generate-test-certs.sh.
    CERT_TEST_NAMES,

    // A certificate will be generated at runtime. A ServerCertificateConfig
    // passed to SetSSLConfig may be used to configure the details of the
    // generated certificate.
    CERT_AUTO,
  };

  enum class IntermediateType {
    // Generated cert is issued directly by the CA.
    kNone,
    // Generated cert is issued by a generated intermediate cert, which is
    // included in the TLS handshake.
    kInHandshake,
    // Generated cert is issued by a generated intermediate, which is NOT
    // included in the TLS handshake, but is available through the leaf's
    // AIA caIssuers URL.
    kByAIA,
  };

  struct OCSPConfig {
    // Enumerates the types of OCSP response that the testserver can produce.
    enum class ResponseType {
      // OCSP will not be enabled for the corresponding config.
      kOff,
      // These correspond to the OCSPResponseStatus enumeration in RFC
      // 6960.
      kSuccessful,
      kMalformedRequest,
      kInternalError,
      kTryLater,
      kSigRequired,
      kUnauthorized,
      // The response will not be valid OCSPResponse DER.
      kInvalidResponse,
      // OCSPResponse will be valid DER but the contained ResponseData will not.
      kInvalidResponseData,
    };

    // OCSPProduced describes the time of the producedAt field in the
    // OCSP response relative to the certificate the response is for.
    enum class Produced {
      // producedAt is between certificate's notBefore and notAfter dates.
      kValid,
      // producedAt is before certificate's notBefore date.
      kBeforeCert,
      // producedAt is after certificate's notAfter date.
      kAfterCert,
    };

    struct SingleResponse {
      // Date describes the thisUpdate..nextUpdate ranges for OCSP
      // singleResponses, relative to the current time.
      enum class Date {
        // The singleResponse is valid for 7 days, and includes the current
        // time.
        kValid,
        // The singleResponse is valid for 7 days, but nextUpdate is before the
        // current time.
        kOld,
        // The singleResponse is valid for 7 days, but thisUpdate is after the
        // current time.
        kEarly,
        // The singleResponse is valid for 366 days, and includes the current
        // time.
        kLong,
        // The singleResponse is valid for 368 days, and includes the current
        // time.
        kLonger,
      };

      // Configures whether a generated OCSP singleResponse's serial field
      // matches the serial number of the target certificate.
      enum class Serial {
        kMatch,
        kMismatch,
      };

      OCSPRevocationStatus cert_status = OCSPRevocationStatus::GOOD;
      Date ocsp_date = Date::kValid;
      Serial serial = Serial::kMatch;
    };

    OCSPConfig();
    // Configure OCSP response with |response_type|.
    explicit OCSPConfig(ResponseType response_type);
    // Configure a successful OCSP response with |single_responses|. |produced|
    // specifies the response's producedAt value, relative to the validity
    // period of the certificate the OCSPConfig is for.
    explicit OCSPConfig(std::vector<SingleResponse> single_responses,
                        Produced produced = Produced::kValid);
    OCSPConfig(const OCSPConfig&);
    OCSPConfig(OCSPConfig&&);
    ~OCSPConfig();
    OCSPConfig& operator=(const OCSPConfig&);
    OCSPConfig& operator=(OCSPConfig&&);

    ResponseType response_type = ResponseType::kOff;
    Produced produced = Produced::kValid;
    std::vector<SingleResponse> single_responses;
  };

  // Configuration for generated server certificate.
  struct ServerCertificateConfig {
    ServerCertificateConfig();
    ServerCertificateConfig(const ServerCertificateConfig&);
    ServerCertificateConfig(ServerCertificateConfig&&);
    ~ServerCertificateConfig();
    ServerCertificateConfig& operator=(const ServerCertificateConfig&);
    ServerCertificateConfig& operator=(ServerCertificateConfig&&);

    // Configure whether the generated certificate chain should include an
    // intermediate, and if so, how it is delivered to the client.
    IntermediateType intermediate = IntermediateType::kNone;

    // Configure OCSP handling.
    // Note: In the current implementation the AIA request handler does not
    // actually parse the OCSP request (a different OCSP URL is used for each
    // cert). So this is useful for testing the client's handling of the OCSP
    // response, but not for testing that the client is sending a proper OCSP
    // request.
    //
    // AIA OCSP for the leaf cert. If |kOff|, no AIA OCSP URL will be included
    // in the leaf cert.
    OCSPConfig ocsp_config;
    // Stapled OCSP for the leaf cert. If |kOff|, OCSP Stapling will not be
    // used.
    OCSPConfig stapled_ocsp_config;
    // AIA OCSP for the intermediate cert. If |kOff|, no AIA OCSP URL will be
    // included in the intermediate cert. It is invalid to supply a
    // configuration other than |kOff| if |intermediate| is |kNone|.
    OCSPConfig intermediate_ocsp_config;

    // Certificate policy OIDs, in text notation (e.g. "1.2.3.4"). If
    // non-empty, the policies will be added to the leaf cert and the
    // intermediate cert (if an intermediate is configured).
    std::vector<std::string> policy_oids;
  };

  typedef base::RepeatingCallback<std::unique_ptr<HttpResponse>(
      const HttpRequest& request)>
      HandleRequestCallback;
  typedef base::RepeatingCallback<void(const HttpRequest& request)>
      MonitorRequestCallback;

  // Creates a http test server. Start() must be called to start the server.
  // |type| indicates the protocol type of the server (HTTP/HTTPS).
  //
  //  When a TYPE_HTTPS server is created, EmbeddedTestServer will call
  // EmbeddedTestServer::RegisterTestCerts(), so that when the default
  // CertVerifiers are run in-process, they will recognize the test server's
  // certs. However, if the test server is running in a different process from
  // the CertVerifiers, EmbeddedTestServer::RegisterTestCerts() must be called
  // in any process where CertVerifiers are expected to accept the
  // EmbeddedTestServer's certs.
  EmbeddedTestServer();
  explicit EmbeddedTestServer(Type type);
  ~EmbeddedTestServer();

  // Registers the EmbeddedTestServer's certs for the current process. See
  // constructor documentation for more information.
  static void RegisterTestCerts();

  // Sets a connection listener, that would be notified when various connection
  // events happen. May only be called before the server is started. Caller
  // maintains ownership of the listener.
  void SetConnectionListener(EmbeddedTestServerConnectionListener* listener);

  // Initializes and waits until the server is ready to accept requests.
  // This is the equivalent of calling InitializeAndListen() followed by
  // StartAcceptingConnections().
  // Returns a "handle" which will ShutdownAndWaitUntilComplete() when
  // destroyed, or null if the listening socket could not be created.
  EmbeddedTestServerHandle StartAndReturnHandle(int port = 0)
      WARN_UNUSED_RESULT;

  // Deprecated equivalent of StartAndReturnHandle().
  bool Start(int port = 0) WARN_UNUSED_RESULT;

  // Starts listening for incoming connections but will not yet accept them.
  // Returns whether a listening socket has been succesfully created.
  bool InitializeAndListen(int port = 0) WARN_UNUSED_RESULT;

  // Starts the Accept IO Thread and begins accepting connections.
  void StartAcceptingConnections();

  // Shuts down the http server and waits until the shutdown is complete.
  bool ShutdownAndWaitUntilComplete() WARN_UNUSED_RESULT;

  // Checks if the server has started listening for incoming connections.
  bool Started() const { return listen_socket_.get() != nullptr; }

  static base::FilePath GetRootCertPemPath();

  HostPortPair host_port_pair() const {
    return HostPortPair::FromURL(base_url_);
  }

  // Returns the base URL to the server, which looks like
  // http://127.0.0.1:<port>/, where <port> is the actual port number used by
  // the server.
  const GURL& base_url() const { return base_url_; }

  // Returns a URL to the server based on the given relative URL, which
  // should start with '/'. For example: GetURL("/path?query=foo") =>
  // http://127.0.0.1:<port>/path?query=foo.
  GURL GetURL(const std::string& relative_url) const;

  // Similar to the above method with the difference that it uses the supplied
  // |hostname| for the URL instead of 127.0.0.1. The hostname should be
  // resolved to 127.0.0.1.
  GURL GetURL(const std::string& hostname,
              const std::string& relative_url) const;

  // Returns the address list needed to connect to the server.
  bool GetAddressList(AddressList* address_list) const WARN_UNUSED_RESULT;

  // Returns the IP Address to connect to the server as a string.
  std::string GetIPLiteralString() const;

  // Returns the port number used by the server.
  uint16_t port() const { return port_; }

  // SetSSLConfig sets the SSL configuration for the server. It is invalid to
  // call after the server is started. If called multiple times, the last call
  // will have effect.
  void SetSSLConfig(ServerCertificate cert, const SSLServerConfig& ssl_config);
  void SetSSLConfig(ServerCertificate cert);
  void SetSSLConfig(const ServerCertificateConfig& cert_config,
                    const SSLServerConfig& ssl_config);
  void SetSSLConfig(const ServerCertificateConfig& cert_config);

  // TODO(mattm): make this WARN_UNUSED_RESULT
  bool ResetSSLConfig(ServerCertificate cert,
                      const SSLServerConfig& ssl_config);

  // Returns the certificate that the server is using.
  // If using a generated ServerCertificate type, this must not be called before
  // InitializeAndListen() has been called.
  scoped_refptr<X509Certificate> GetCertificate();

  // Registers request handler which serves files from |directory|.
  // For instance, a request to "/foo.html" is served by "foo.html" under
  // |directory|. Files under sub directories are also handled in the same way
  // (i.e. "/foo/bar.html" is served by "foo/bar.html" under |directory|).
  // TODO(svaldez): Merge ServeFilesFromDirectory and
  // ServeFilesFromSourceDirectory.
  void ServeFilesFromDirectory(const base::FilePath& directory);

  // Serves files relative to DIR_SOURCE_ROOT.
  void ServeFilesFromSourceDirectory(const std::string& relative);
  void ServeFilesFromSourceDirectory(const base::FilePath& relative);

  // Registers the default handlers and serve additional files from the
  // |directory| directory, relative to DIR_SOURCE_ROOT.
  void AddDefaultHandlers(const base::FilePath& directory);

  // The most general purpose method. Any request processing can be added using
  // this method. Takes ownership of the object. The |callback| is called
  // on the server's IO thread so all handlers must be registered before the
  // server is started.
  void RegisterRequestHandler(const HandleRequestCallback& callback);

  // Adds request monitors. The |callback| is called before any handlers are
  // called, but can not respond it. This is useful to monitor requests that
  // will be handled by other request handlers. The |callback| is called
  // on the server's IO thread so all monitors must be registered before the
  // server is started.
  void RegisterRequestMonitor(const MonitorRequestCallback& callback);

  // Adds default handlers, including those added by AddDefaultHandlers, to be
  // tried after all other user-specified handlers have been tried. The
  // |callback| is called on the server's IO thread so all handlers must be
  // registered before the server is started.
  void RegisterDefaultHandler(const HandleRequestCallback& callback);

  bool FlushAllSocketsAndConnectionsOnUIThread();
  void FlushAllSocketsAndConnections();

 private:
  // Returns the file name of the certificate the server is using. The test
  // certificates can be found in net/data/ssl/certificates/.
  std::string GetCertificateName() const;

  // Shuts down the server.
  void ShutdownOnIOThread();

  // Sets the SSL configuration for the server. It is invalid for |cert_config|
  // to be non-null if |cert| is not CERT_AUTO.
  void SetSSLConfigInternal(ServerCertificate cert,
                            const ServerCertificateConfig* cert_config,
                            const SSLServerConfig& ssl_config);

  // Resets the SSLServerConfig on the IO thread.
  bool ResetSSLConfigOnIOThread(ServerCertificate cert,
                                const SSLServerConfig& ssl_config);

  // Upgrade the TCP connection to one over SSL.
  std::unique_ptr<StreamSocket> DoSSLUpgrade(
      std::unique_ptr<StreamSocket> connection);
  // Handles async callback when the SSL handshake has been completed.
  void OnHandshakeDone(HttpConnection* connection, int rv);

  // Begins accepting new client connections.
  void DoAcceptLoop();
  // Handles async callback when there is a new client socket. |rv| is the
  // return value of the socket Accept.
  void OnAcceptCompleted(int rv);
  // Adds the new |socket| to the list of clients and begins the reading
  // data.
  void HandleAcceptResult(std::unique_ptr<StreamSocket> socket);

  // Attempts to read data from the |connection|'s socket.
  void ReadData(HttpConnection* connection);
  // Handles async callback when new data has been read from the |connection|.
  void OnReadCompleted(HttpConnection* connection, int rv);
  // Parses the data read from the |connection| and returns true if the entire
  // request has been received.
  bool HandleReadResult(HttpConnection* connection, int rv);

  // Called when |connection| is finished writing the response and the socket
  // can be closed, allowing for |connnection_listener_| to take it if the
  // socket is still open.
  void OnResponseCompleted(HttpConnection* connection);

  // Closes and removes the connection upon error or completion.
  void DidClose(HttpConnection* connection);

  // Handles a request when it is parsed. It passes the request to registered
  // request handlers and sends a http response.
  void HandleRequest(HttpConnection* connection,
                     std::unique_ptr<HttpRequest> request);

  // Returns true if the current |cert_| configuration uses a static
  // pre-generated cert loaded from the filesystem.
  bool UsingStaticCert() const;

  // Reads server certificate and private key from file. May only be called if
  // |cert_| refers to a file-based cert & key.
  bool InitializeCertAndKeyFromFile() WARN_UNUSED_RESULT;

  // Generate server certificate and private key. May only be called if |cert_|
  // refers to a generated cert & key.
  bool GenerateCertAndKey() WARN_UNUSED_RESULT;

  // Initializes the SSLServerContext so that SSLServerSocket connections may
  // share the same cache
  bool InitializeSSLServerContext() WARN_UNUSED_RESULT;

  HttpConnection* FindConnection(StreamSocket* socket);

  // Posts a task to the |io_thread_| and waits for a reply.
  bool PostTaskToIOThreadAndWait(base::OnceClosure closure) WARN_UNUSED_RESULT;

  // Posts a task that returns a true/false success/fail value to the
  // |io_thread_| and waits for a reply.
  bool PostTaskToIOThreadAndWaitWithResult(base::OnceCallback<bool()> task)
      WARN_UNUSED_RESULT;

  const bool is_using_ssl_;

  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<TCPServerSocket> listen_socket_;
  std::unique_ptr<StreamSocket> accepted_socket_;

  EmbeddedTestServerConnectionListener* connection_listener_;
  uint16_t port_;
  GURL base_url_;
  IPEndPoint local_endpoint_;

  std::map<StreamSocket*, std::unique_ptr<HttpConnection>> connections_;

  // Vector of registered and default request handlers and monitors.
  std::vector<HandleRequestCallback> request_handlers_;
  std::vector<MonitorRequestCallback> request_monitors_;
  std::vector<HandleRequestCallback> default_request_handlers_;

  base::ThreadChecker thread_checker_;

  net::SSLServerConfig ssl_config_;
  ServerCertificate cert_;
  ServerCertificateConfig cert_config_;
  scoped_refptr<X509Certificate> x509_cert_;
  bssl::UniquePtr<EVP_PKEY> private_key_;
  std::unique_ptr<SSLServerContext> context_;

  // HTTP server that handles AIA URLs that are embedded in this test server's
  // certificate when the server certificate is one of the CERT_AUTO variants.
  std::unique_ptr<EmbeddedTestServer> aia_http_server_;

  base::WeakPtrFactory<EmbeddedTestServer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServer);
};

class EmbeddedTestServerHandle {
 public:
  EmbeddedTestServerHandle() = default;
  EmbeddedTestServerHandle(EmbeddedTestServerHandle&& other);
  EmbeddedTestServerHandle& operator=(EmbeddedTestServerHandle&& other);

  ~EmbeddedTestServerHandle();

  explicit operator bool() const { return test_server_; }

 private:
  friend class EmbeddedTestServer;

  explicit EmbeddedTestServerHandle(EmbeddedTestServer* test_server);
  EmbeddedTestServer* test_server_ = nullptr;
};

}  // namespace test_server

// TODO(svaldez): Refactor EmbeddedTestServer to be in the net namespace.
using test_server::EmbeddedTestServer;

}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
