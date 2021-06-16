// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_URL_LOADER_FACTORY_GETTER_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_URL_LOADER_FACTORY_GETTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/sequenced_task_runner_helpers.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace net_service {

struct URLLoaderFactoryGetterDeleter;

// Helper class for retrieving a URLLoaderFactory that can be bound on any
// thread, and that correctly handles proxied requests.
class URLLoaderFactoryGetter
    : public base::RefCountedThreadSafe<URLLoaderFactoryGetter,
                                        URLLoaderFactoryGetterDeleter> {
 public:
  // Create a URLLoaderFactoryGetter on the UI thread.
  // |render_frame_host| may be nullptr.
  static scoped_refptr<URLLoaderFactoryGetter> Create(
      content::RenderFrameHost* render_frame_host,
      content::BrowserContext* browser_context);

  // Create a SharedURLLoaderFactory on the current thread. All future calls
  // to this method must be on the same thread.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

 private:
  friend class base::DeleteHelper<URLLoaderFactoryGetter>;
  friend class base::RefCountedThreadSafe<URLLoaderFactoryGetter,
                                          URLLoaderFactoryGetterDeleter>;
  friend struct URLLoaderFactoryGetterDeleter;

  URLLoaderFactoryGetter(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          loader_factory_info,
      network::mojom::URLLoaderFactoryPtrInfo proxy_factory_ptr_info,
      network::mojom::URLLoaderFactoryRequest proxy_factory_request);
  ~URLLoaderFactoryGetter();

  void DeleteOnCorrectThread() const;

  std::unique_ptr<network::PendingSharedURLLoaderFactory> loader_factory_info_;
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory_;
  network::mojom::URLLoaderFactoryPtrInfo proxy_factory_ptr_info_;
  network::mojom::URLLoaderFactoryRequest proxy_factory_request_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryGetter);
};

struct URLLoaderFactoryGetterDeleter {
  static void Destruct(const URLLoaderFactoryGetter* factory_getter) {
    factory_getter->DeleteOnCorrectThread();
  }
};

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_URL_LOADER_FACTORY_GETTER_H_
