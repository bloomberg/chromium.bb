// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_FACTORY_BUNDLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_FACTORY_BUNDLE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}

namespace blink {

// Holds the internal state of a URLLoaderFactoryBundle in a form that is safe
// to pass across sequences.
class BLINK_COMMON_EXPORT PendingURLLoaderFactoryBundle
    : public network::PendingSharedURLLoaderFactory {
 public:
  // Map from URL scheme to PendingRemote<URLLoaderFactory> for handling URL
  // requests for schemes not handled by the |pending_default_factory|. See also
  // URLLoaderFactoryBundle::SchemeMap.
  using SchemeMap =
      std::map<std::string,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>>;

  // Map from origin of isolated world to PendingRemote<URLLoaderFactory> for
  // handling this isolated world's requests (e.g., for relaxing CORB for
  // requests initiated from content scripts).
  using OriginMap =
      std::map<url::Origin,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>>;

  PendingURLLoaderFactoryBundle();
  PendingURLLoaderFactoryBundle(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_default_factory,
      SchemeMap scheme_specific_pending_factories,
      OriginMap isolated_world_pending_factories,
      bool bypass_redirect_checks);
  ~PendingURLLoaderFactoryBundle() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  pending_default_factory() {
    return pending_default_factory_;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  pending_appcache_factory() {
    return pending_appcache_factory_;
  }

  SchemeMap& pending_scheme_specific_factories() {
    return pending_scheme_specific_factories_;
  }
  OriginMap& pending_isolated_world_factories() {
    return pending_isolated_world_factories_;
  }

  bool bypass_redirect_checks() const { return bypass_redirect_checks_; }
  void set_bypass_redirect_checks(bool bypass_redirect_checks) {
    bypass_redirect_checks_ = bypass_redirect_checks;
  }

 protected:
  // PendingSharedURLLoaderFactory implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_default_factory_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_appcache_factory_;
  SchemeMap pending_scheme_specific_factories_;
  OriginMap pending_isolated_world_factories_;
  bool bypass_redirect_checks_ = false;

  DISALLOW_COPY_AND_ASSIGN(PendingURLLoaderFactoryBundle);
};

// Encapsulates a collection of URLLoaderFactoryPtrs which can be usd to acquire
// loaders for various types of resource requests.
class BLINK_COMMON_EXPORT URLLoaderFactoryBundle
    : public network::SharedURLLoaderFactory {
 public:
  URLLoaderFactoryBundle();

  explicit URLLoaderFactoryBundle(
      std::unique_ptr<PendingURLLoaderFactoryBundle> pending_factories);

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;
  bool BypassRedirectChecks() const override;

  // The |pending_factories| contains replacement factories for a subset of the
  // existing bundle.
  void Update(std::unique_ptr<PendingURLLoaderFactoryBundle> pending_factories);

 protected:
  ~URLLoaderFactoryBundle() override;

  // Returns a factory which can be used to acquire a loader for |request|.
  virtual network::mojom::URLLoaderFactory* GetFactory(
      const network::ResourceRequest& request);

  template <typename TKey>
  static std::map<TKey, mojo::PendingRemote<network::mojom::URLLoaderFactory>>
  CloneRemoteMapToPendingRemoteMap(
      const std::map<TKey, mojo::Remote<network::mojom::URLLoaderFactory>>&
          input) {
    std::map<TKey, mojo::PendingRemote<network::mojom::URLLoaderFactory>>
        output;
    for (const auto& it : input) {
      const TKey& key = it.first;
      const mojo::Remote<network::mojom::URLLoaderFactory>& factory = it.second;
      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory;
      factory->Clone(pending_factory.InitWithNewPipeAndPassReceiver());
      output.emplace(key, std::move(pending_factory));
    }
    return output;
  }

  // |default_factory_| is the default factory used by the bundle. It usually
  // goes to "network", but it's possible it was overriden in case when the
  // context should not be given access to the network.
  mojo::Remote<network::mojom::URLLoaderFactory> default_factory_;

  // |appcache_factory_| is a special loader factory that intercepts
  // requests when the context has AppCache. See also
  // AppCacheSubresourceURLFactory.
  mojo::Remote<network::mojom::URLLoaderFactory> appcache_factory_;

  // Map from URL scheme to Remote<URLLoaderFactory> for handling URL requests
  // for schemes not handled by the |default_factory_|.  See also
  // PendingURLLoaderFactoryBundle::SchemeMap and
  // ContentBrowserClient::SchemeToURLLoaderFactoryMap.
  using SchemeMap =
      std::map<std::string, mojo::Remote<network::mojom::URLLoaderFactory>>;
  SchemeMap scheme_specific_factories_;

  // Map from origin of isolated world to Remote<URLLoaderFactory> for handling
  // this isolated world's requests. See also
  // PendingURLLoaderFactoryBundle::OriginMap.
  using OriginMap =
      std::map<url::Origin, mojo::Remote<network::mojom::URLLoaderFactory>>;
  OriginMap isolated_world_factories_;

  bool bypass_redirect_checks_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_FACTORY_BUNDLE_H_
