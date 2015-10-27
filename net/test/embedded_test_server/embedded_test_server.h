// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace net {

class StreamSocket;
class TCPServerSocket;

namespace test_server {

class EmbeddedTestServerConnectionListener;
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
//   test_server_.reset(new EmbeddedTestServer());
//   ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
//   test_server_->RegisterRequestHandler(
//       base::Bind(&FooTest::HandleRequest, base::Unretained(this)));
// }
//
// scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
//   GURL absolute_url = test_server_->GetURL(request.relative_url);
//   if (absolute_url.path() != "/test")
//     return scoped_ptr<HttpResponse>();
//
//   scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
//   http_response->set_code(test_server::SUCCESS);
//   http_response->set_content("hello");
//   http_response->set_content_type("text/plain");
//   return http_response.Pass();
// }
//
// For a test that spawns another process such as browser_tests, it is
// suggested to call InitializeAndWaitUntilReady in SetUpOnMainThread after
// the process is spawned. If you have to do it before the process spawns,
// you need to first setup the listen socket so that there is no no other
// threads running while spawning the process. To do so, please follow
// the following example:
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
  typedef base::Callback<scoped_ptr<HttpResponse>(
      const HttpRequest& request)> HandleRequestCallback;

  // Creates a http test server. InitializeAndWaitUntilReady() must be called
  // to start the server.
  EmbeddedTestServer();
  ~EmbeddedTestServer();

  // Sets a connection listener, that would be notified when various connection
  // events happen. May only be called before the server is started. Caller
  // maintains ownership of the listener.
  void SetConnectionListener(EmbeddedTestServerConnectionListener* listener);

  // Initializes and waits until the server is ready to accept requests.
  // This is the equivalent of calling InitializeAndListen() followed by
  // StartAcceptingConnections().
  // Returns whether a listening socket has been succesfully created.
  bool InitializeAndWaitUntilReady() WARN_UNUSED_RESULT;

  // Starts listening for incoming connections but will not yet accept them.
  // Returns whether a listening socket has been succesfully created.
  bool InitializeAndListen() WARN_UNUSED_RESULT;

  // Starts the Accept IO Thread and begins accepting connections.
  void StartAcceptingConnections();

  // Shuts down the http server and waits until the shutdown is complete.
  bool ShutdownAndWaitUntilComplete() WARN_UNUSED_RESULT;

  // Checks if the server has started listening for incoming connections.
  bool Started() const {
    return listen_socket_.get() != NULL;
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
  bool GetAddressList(net::AddressList* address_list) const WARN_UNUSED_RESULT;

  // Returns the port number used by the server.
  uint16 port() const { return port_; }

  // Registers request handler which serves files from |directory|.
  // For instance, a request to "/foo.html" is served by "foo.html" under
  // |directory|. Files under sub directories are also handled in the same way
  // (i.e. "/foo/bar.html" is served by "foo/bar.html" under |directory|).
  void ServeFilesFromDirectory(const base::FilePath& directory);

  // The most general purpose method. Any request processing can be added using
  // this method. Takes ownership of the object. The |callback| is called
  // on UI thread.
  void RegisterRequestHandler(const HandleRequestCallback& callback);

 private:
  // Shuts down the server.
  void ShutdownOnIOThread();

  // Begins accepting new client connections.
  void DoAcceptLoop();
  // Handles async callback when there is a new client socket. |rv| is the
  // return value of the socket Accept.
  void OnAcceptCompleted(int rv);
  // Adds the new |socket| to the list of clients and begins the reading
  // data.
  void HandleAcceptResult(scoped_ptr<StreamSocket> socket);

  // Attempts to read data from the |connection|'s socket.
  void ReadData(HttpConnection* connection);
  // Handles async callback when new data has been read from the |connection|.
  void OnReadCompleted(HttpConnection* connection, int rv);
  // Parses the data read from the |connection| and returns true if the entire
  // request has been received.
  bool HandleReadResult(HttpConnection* connection, int rv);

  // Closes and removes the connection upon error or completion.
  void DidClose(HttpConnection* connection);

  // Handles a request when it is parsed. It passes the request to registered
  // request handlers and sends a http response.
  void HandleRequest(HttpConnection* connection,
                     scoped_ptr<HttpRequest> request);

  HttpConnection* FindConnection(StreamSocket* socket);

  // Posts a task to the |io_thread_| and waits for a reply.
  bool PostTaskToIOThreadAndWait(
      const base::Closure& closure) WARN_UNUSED_RESULT;

  scoped_ptr<base::Thread> io_thread_;

  scoped_ptr<TCPServerSocket> listen_socket_;
  scoped_ptr<StreamSocket> accepted_socket_;

  EmbeddedTestServerConnectionListener* connection_listener_;
  uint16 port_;
  GURL base_url_;
  IPEndPoint local_endpoint_;

  // Owns the HttpConnection objects.
  std::map<StreamSocket*, HttpConnection*> connections_;

  // Vector of registered request handlers.
  std::vector<HandleRequestCallback> request_handlers_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServer);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
