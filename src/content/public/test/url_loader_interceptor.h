// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_URL_LOADER_INTERCEPTOR_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// Helper class to intercept URLLoaderFactory calls for tests.
// This intercepts:
//   -frame requests (which start from the browser)
//   -subresource requests from pages and dedicad workers and shared workers.
//     -at ResourceMessageFilter for non network-service code path
//     -by sending renderer an intermediate URLLoaderFactory for network-service
//      code path, as that normally routes directly to the network process
//   -subresource requests from service workers and requests of non-installed
//    service worker scripts
//     -at ResourceMessageFilter for non network-service code path
//     -at EmbeddedWorkerInstance for network-service code path.
//   -requests by the browser
//
//   -http(s)://mock.failed.request/foo URLs internally, copying the behavior
//    of net::URLRequestFailedJob
//
// Prefer not to use this class. In order of ease of use & simplicity:
//  -if you need to serve static data, use net::test::EmbeddedTestServer and
//   serve data from the source tree (e.g. in content/test/data)
//  -if you need to control the response data at runtime, then use
//   net::test_server::EmbeddedTestServer::RegisterRequestHandler
//  -if you need to delay when the server sends the response, use
//   net::test_server::ControllableHttpResponse
//  -otherwise, if you need full control over the net::Error and/or want to
//   inspect and/or modify the C++ structs used by URLoader interface, then use
//   this helper class
//
// Notes:
//  -the callback is called on the UI or IO threads depending on the factory
//   that was hooked
//    -this is done to avoid changing message order
//  -intercepting resource requests for subresources when the network service is
//   enabled changes message order by definition (since they would normally go
//   directly from renderer->network process, but now they're routed through the
//   browser).
class URLLoaderInterceptor {
 public:
  struct RequestParams {
    RequestParams();
    ~RequestParams();
    RequestParams(RequestParams&& other);
    RequestParams& operator=(RequestParams&& other);
    // This is the process_id of the process that is making the request (0 for
    // browser process).
    int process_id;
    // The following are the parameters to CreateLoaderAndStart.
    mojo::PendingReceiver<network::mojom::URLLoader> receiver;
    int32_t routing_id;
    int32_t request_id;
    uint32_t options;
    network::ResourceRequest url_request;
    mojo::Remote<network::mojom::URLLoaderClient> client;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };
  // Function signature for intercept method.
  // Return true if the request was intercepted. Otherwise this class will
  // forward the request to the original URLLoaderFactory.
  using InterceptCallback =
      base::RepeatingCallback<bool(RequestParams* params)>;

  // Function signature for a loading completion method.
  // This class will listen on loading completion responses from the network,
  // invoke this callback, and delegate the response to the original client.
  using URLLoaderCompletionStatusCallback = base::RepeatingCallback<void(
      const GURL& request_url,
      const network::URLLoaderCompletionStatus& status)>;

  // Create an interceptor which calls |callback|. If |ready_callback| is not
  // provided, a nested RunLoop is used to ensure the interceptor is ready
  // before returning. If |ready_callback| is provided, no RunLoop is called,
  // and instead |ready_callback| is called after the interceptor is installed.
  // If provided, |completion_status_callback| is called when the load
  // completes.
  explicit URLLoaderInterceptor(InterceptCallback callback);
  URLLoaderInterceptor(
      InterceptCallback callback,
      const URLLoaderCompletionStatusCallback& completion_status_callback,
      base::OnceClosure ready_callback);

  ~URLLoaderInterceptor();

  // Helper methods for use when intercepting.
  // Writes the given response body, header, and SSL Info to |client|.
  static void WriteResponse(
      const std::string& headers,
      const std::string& body,
      network::mojom::URLLoaderClient* client,
      base::Optional<net::SSLInfo> ssl_info = base::nullopt);

  // Reads the given path, relative to the root source directory, and writes it
  // to |client|. For headers:
  //   1) if |headers| is specified, it's used
  //   2) otherwise if an adjoining file that ends in .mock-http-headers is
  //      found, its contents will be used
  //   3) otherwise a simple 200 response will be used, with a Content-Type
  //      guessed from the file extension
  // For SSL info, if |ssl_info| is specified, then it is added to the response.
  static void WriteResponse(
      const std::string& relative_path,
      network::mojom::URLLoaderClient* client,
      const std::string* headers = nullptr,
      base::Optional<net::SSLInfo> ssl_info = base::nullopt);

  // Like above, but uses an absolute file path.
  static void WriteResponse(
      const base::FilePath& file_path,
      network::mojom::URLLoaderClient* client,
      const std::string* headers = nullptr,
      base::Optional<net::SSLInfo> ssl_info = base::nullopt);

  // Returns an interceptor that (as long as it says alive) will intercept
  // requests to |url| and fail them using the provided |error|.
  // |ready_callback| is optional and avoids the use of RunLoop, see
  // the constructor for more detail.
  static std::unique_ptr<URLLoaderInterceptor> SetupRequestFailForURL(
      const GURL& url,
      net::Error error,
      base::OnceClosure ready_callback = {});

 private:
  class BrowserProcessWrapper;
  class Interceptor;
  class IOState;
  class RenderProcessHostWrapper;
  class URLLoaderFactoryGetterWrapper;
  class URLLoaderFactoryNavigationWrapper;

  // Used to create a factory associated with a specific RenderProcessHost.
  void CreateURLLoaderFactoryForRenderProcessHost(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      int process_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory);

  // Callback on UI thread whenever a
  // StoragePartition::GetURLLoaderFactoryForBrowserProcess is called on an
  // object that doesn't have a test factory set up.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory);

  // Callback on IO thread whenever a NavigationURLLoaderImpl is loading a frame
  // request through ResourceDispatcherHost (i.e. when the network service is
  // disabled).
  bool BeginNavigationCallback(
      mojo::PendingReceiver<network::mojom::URLLoader>* receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient>* client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Callback on UI thread whenever NavigationURLLoaderImpl needs a
  // URLLoaderFactory with a network::mojom::TrustedURLLoaderHeaderClient or
  // for a non-network-service scheme.
  void InterceptNavigationRequestCallback(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* receiver);

  // Attempts to intercept the given request, returning true if it was
  // intercepted.
  bool Intercept(RequestParams* params);

  // Called on IO thread at initialization and shutdown.
  void InitializeOnIOThread(base::OnceClosure closure);

  bool use_runloop_;
  base::OnceClosure ready_callback_;
  InterceptCallback callback_;
  scoped_refptr<IOState> io_thread_;
  // For intecepting non-frame requests from the browser process. There is one
  // per StoragePartition. Only accessed on UI thread.
  std::set<std::unique_ptr<BrowserProcessWrapper>>
      browser_process_interceptors_;

  std::set<std::unique_ptr<URLLoaderFactoryNavigationWrapper>>
      navigation_wrappers_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderInterceptor);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_URL_LOADER_INTERCEPTOR_H_
