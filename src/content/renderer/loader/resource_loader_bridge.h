/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_CONTENT_RESOURCE_LOADER_BRIDGE_H
#define INCLUDED_CONTENT_RESOURCE_LOADER_BRIDGE_H

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/renderer/request_peer.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"

namespace content {
struct SyncLoadResponse;

// ResourceRequestInfoProvider is the base class to provide resource request.
// The request information is used by the in-process resource loader.
// https://chromium.googlesource.com/chromium/src/+/00fd5bbe7fb266480a80b54940ce072d2f20eda2
// explains why WebNavigationBodyLoader is created. Now there are two ways to
// load resources: WebNavigationBodyLoader and WebUrlLoader respectively. To
// deal with both cases, ResourceRequestInfoProvider and ResourceReceiver are
// defined to serve as the base classes for requester and receiver,
// respectively.
class CONTENT_EXPORT ResourceRequestInfoProvider {
 public:
  ResourceRequestInfoProvider();
  virtual ~ResourceRequestInfoProvider();

  const GURL& url() const;
  const GURL& firstPartyForCookies() const;
  bool allowStoredCredentials() const;
  int loadFlags() const;
  const std::string& httpMethod() const;
  const net::HttpRequestHeaders& requestHeaders() const;
  bool reportUploadProgress() const;
  bool reportRawHeaders() const;
  bool hasUserGesture() const;
  int routingId() const;
  base::Optional<base::UnguessableToken> appCacheHostId() const;
  net::RequestPriority priority() const;
  scoped_refptr<network::ResourceRequestBody> requestBody() const;

 protected:
  GURL url_;
  GURL firstPartyForCookies_;
  bool allowStoredCredentials_;
  int loadFlags_;
  std::string httpMethod_;
  std::string httpHeaderField_;
  net::HttpRequestHeaders requestHeaders_;
  bool reportUploadProgress_;
  bool reportRawHeaders_;
  bool hasUserGesture_;
  int routingId_;
  base::Optional<base::UnguessableToken> appCacheHostId_;
  net::RequestPriority priority_;
  scoped_refptr<network::ResourceRequestBody> requestBody_;
};

// ResourceReceiver is the base class for resource receiving sink. It is paired
// with ResourceRequestInfoProvider for in-process resource requisition.
class CONTENT_EXPORT ResourceReceiver {
 public:
  ResourceReceiver();
  virtual ~ResourceReceiver();
  // Called when response headers are available (after all redirects have
  // been followed).
  virtual void OnReceivedResponse(
      const network::ResourceResponseInfo& info) = 0;

  // Called when a chunk of response data is available. This method may
  // be called multiple times or not at all if an error occurs.
  virtual void OnReceivedData(const char* data, std::size_t data_length) = 0;

  // Called when the response is complete.  This method signals completion of
  // the resource load.
  virtual void OnCompletedRequest(const network::URLLoaderCompletionStatus&,
                                  const GURL&) = 0;
};

// BodyLoaderRequestInfoProvider provides in-process resource request
// information for navigation body loader. See its usage in
// content::NavigationBodyLoader and content::RenderFrameImpl.
class CONTENT_EXPORT BodyLoaderRequestInfoProvider
    : public ResourceRequestInfoProvider {
 public:
  BodyLoaderRequestInfoProvider(
      const CommonNavigationParams& common_params,
      const CommitNavigationParams& commit_params,
      const network::ResourceResponseHead& head,
      const network::mojom::URLLoaderClientEndpointsPtr& client_endpoints,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      int render_frame_id,
      const mojom::ResourceLoadInfoPtr& resource_load_info);
  ~BodyLoaderRequestInfoProvider() override;
};

// PeerRequestInfoProvider provides in-process resource request
// information for web url loader. See its usage in content::ResourceDispatcher
class CONTENT_EXPORT PeerRequestInfoProvider
    : public ResourceRequestInfoProvider {
 public:
  PeerRequestInfoProvider(const network::ResourceRequest* request);
  ~PeerRequestInfoProvider() override;
};

// BodyLoaderReceiver serves as the resource receiver for in-process navigation
// body loader. It is paired with BodyLoaderRequestInfoProvider. See its usage
// in content::NavigationBodyLoader and content::RenderFrameImpl.
class CONTENT_EXPORT BodyLoaderReceiver : public ResourceReceiver {
 public:
  BodyLoaderReceiver(int render_frame_id,
                     blink::WebNavigationBodyLoader::Client* client);
  ~BodyLoaderReceiver() override;

  void OnReceivedResponse(const network::ResourceResponseInfo& info) override;
  void OnReceivedData(const char* data, std::size_t data_length) override;
  void OnCompletedRequest(const network::URLLoaderCompletionStatus&,
                          const GURL&) override;

 private:
  bool IsClientValid() const;
  int render_frame_id_;
  blink::WebNavigationBodyLoader::Client* client_;
};

// RequestPeerReceiver serves as the resource receiver for in-process web url
// loader. It is paired with PeerRequestInfoProvider. See its usage in
// content::ResourceDispatcher
class CONTENT_EXPORT RequestPeerReceiver : public ResourceReceiver {
 public:
  using FinishCallback = std::function<void()>;
  RequestPeerReceiver(content::RequestPeer* peer,
                      int request_id,
                      scoped_refptr<base::SingleThreadTaskRunner> runner);
  ~RequestPeerReceiver() override;

  void OnReceivedResponse(const network::ResourceResponseInfo& info) override;
  void OnReceivedData(const char* data, std::size_t data_length) override;
  void OnCompletedRequest(const network::URLLoaderCompletionStatus&,
                          const GURL&) override;

 private:
  content::RequestPeer* peer_;
  int request_id_;
  scoped_refptr<base::SingleThreadTaskRunner> runner_;
};

// ResourceLoaderBridge is a base class to provide interfaces for in-process
// resource requisition.
class CONTENT_EXPORT ResourceLoaderBridge
    : public blink::WebNavigationBodyLoader {
 public:
  ResourceLoaderBridge(const content::ResourceRequestInfoProvider&);
  ~ResourceLoaderBridge() override;

  // Start, Cancel, SyncLoad will be called in content::ResourceDispatcher
  virtual void Start(std::unique_ptr<ResourceReceiver> receiver) = 0;
  virtual void Cancel() = 0;
  virtual void SyncLoad(content::SyncLoadResponse* response) = 0;
};

}  // namespace content

#endif  // INCLUDED_CONTENT_RESOURCE_LOADER_BRIDGE_H
