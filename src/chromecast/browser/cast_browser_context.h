// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"

namespace chromecast {
namespace shell {

class URLRequestContextFactory;

// Chromecast does not currently support multiple profiles.  So there is a
// single BrowserContext for all chromecast renderers.
// There is no support for PartitionStorage.
class CastBrowserContext final : public content::BrowserContext {
 public:
  explicit CastBrowserContext(
      URLRequestContextFactory* url_request_context_factory);
  ~CastBrowserContext() override;

  // BrowserContext implementation:
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif  // !defined(OS_ANDROID)
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

  net::URLRequestContextGetter* GetSystemRequestContext();

  void SetCorsOriginAccessListForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override;
  const content::SharedCorsOriginAccessList* GetSharedCorsOriginAccessList()
      const override;

 private:
  class CastResourceContext;

  // Performs initialization of the CastBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  URLRequestContextFactory* const url_request_context_factory_;
  base::FilePath path_;
  std::unique_ptr<CastResourceContext> resource_context_;
  std::unique_ptr<content::PermissionControllerDelegate> permission_manager_;
  std::unique_ptr<SimpleFactoryKey> simple_factory_key_;
  scoped_refptr<content::SharedCorsOriginAccessList>
      shared_cors_origin_access_list_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserContext);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_
