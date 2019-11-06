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
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}

namespace blink {

// Holds the internal state of a URLLoaderFactoryBundle in a form that is safe
// to pass across sequences.
// TODO(domfarolino, crbug.com/955171): This class should be renamed to not
// include "Info".
class BLINK_COMMON_EXPORT URLLoaderFactoryBundleInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  // Map from URL scheme to PendingRemote<URLLoaderFactory> for handling URL
  // requests for schemes not handled by the |pending_default_factory|. See also
  // URLLoaderFactoryBundle::SchemeMap.
  using SchemeMap =
      std::map<std::string,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>>;

  // Map from origin of request initiator to PendingRemote<URLLoaderFactory> for
  // handling this initiator's requests (e.g., for relaxing CORB for requests
  // initiated from content scripts).
  using OriginMap =
      std::map<url::Origin,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>>;

  URLLoaderFactoryBundleInfo();
  URLLoaderFactoryBundleInfo(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_default_factory,
      SchemeMap scheme_specific_factory_infos,
      OriginMap initiator_specific_factory_infos,
      bool bypass_redirect_checks);
  ~URLLoaderFactoryBundleInfo() override;

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
  OriginMap& pending_initiator_specific_factories() {
    return pending_initiator_specific_factories_;
  }

  bool bypass_redirect_checks() const { return bypass_redirect_checks_; }
  void set_bypass_redirect_checks(bool bypass_redirect_checks) {
    bypass_redirect_checks_ = bypass_redirect_checks;
  }

 protected:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_default_factory_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_appcache_factory_;
  SchemeMap pending_scheme_specific_factories_;
  OriginMap pending_initiator_specific_factories_;
  bool bypass_redirect_checks_ = false;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryBundleInfo);
};

// Encapsulates a collection of URLLoaderFactoryPtrs which can be usd to acquire
// loaders for various types of resource requests.
class BLINK_COMMON_EXPORT URLLoaderFactoryBundle
    : public network::SharedURLLoaderFactory {
 public:
  URLLoaderFactoryBundle();

  explicit URLLoaderFactoryBundle(
      std::unique_ptr<URLLoaderFactoryBundleInfo> pending_factories);

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override;
  bool BypassRedirectChecks() const override;

  // The |pending_factories| contains replacement factories for a subset of the
  // existing bundle.
  void Update(std::unique_ptr<URLLoaderFactoryBundleInfo> pending_factories);

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

  // TODO(crbug.com/955171): Replace URLLoaderFactoryPtr with Remote below.
  // |default_factory_| is the default factory used by the bundle. It usually
  // goes to "network", but it's possible it was overriden in case when the
  // context should not be given access to the network.
  network::mojom::URLLoaderFactoryPtr default_factory_;

  // |appcache_factory_| is a special loader factory that intercepts
  // requests when the context has AppCache. See also
  // AppCacheSubresourceURLFactory.
  network::mojom::URLLoaderFactoryPtr appcache_factory_;

  // Map from URL scheme to Remote<URLLoaderFactory> for handling URL requests
  // for schemes not handled by the |default_factory_|.  See also
  // URLLoaderFactoryBundleInfo::SchemeMap and
  // ContentBrowserClient::SchemeToURLLoaderFactoryMap.
  using SchemeMap =
      std::map<std::string, mojo::Remote<network::mojom::URLLoaderFactory>>;
  SchemeMap scheme_specific_factories_;

  // Map from origin of request initiator to Remote<URLLoaderFactory> for
  // handling this initiator's requests. See also
  // URLLoaderFactoryBundleInfo::OriginMap.
  using OriginMap =
      std::map<url::Origin, mojo::Remote<network::mojom::URLLoaderFactory>>;
  OriginMap initiator_specific_factories_;

  bool bypass_redirect_checks_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_FACTORY_BUNDLE_H_
