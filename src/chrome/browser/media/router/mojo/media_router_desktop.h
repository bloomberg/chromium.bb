// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/media/router/mojo/media_sink_service_status.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media_router {
class CastMediaRouteProvider;
class DualMediaSinkService;
class WiredDisplayMediaRouteProvider;

// MediaRouter implementation that uses the desktop MediaRouteProviders.
class MediaRouterDesktop : public MediaRouterMojoImpl {
 public:
  // This constructor performs a firewall check on Windows and is not suitable
  // for use in unit tests; instead use the constructor below.
  explicit MediaRouterDesktop(content::BrowserContext* context);

  MediaRouterDesktop(const MediaRouterDesktop&) = delete;
  MediaRouterDesktop& operator=(const MediaRouterDesktop&) = delete;

  ~MediaRouterDesktop() override;

  // MediaRouter implementation.
  void OnUserGesture() override;
  base::Value GetState() const override;
  void GetProviderState(
      mojom::MediaRouteProviderId provider_id,
      mojom::MediaRouteProvider::GetStateCallback callback) const override;

 protected:
  // MediaRouterMojoImpl override:
  absl::optional<mojom::MediaRouteProviderId> GetProviderIdForPresentation(
      const std::string& presentation_id) override;

 private:
  friend class MediaRouterDesktopTest;

  // Used by tests only. This constructor skips the firewall check so unit tests
  // do not have to depend on the system's firewall configuration.
  MediaRouterDesktop(content::BrowserContext* context,
                     DualMediaSinkService* media_sink_service);

  // mojom::MediaRouter implementation.
  void RegisterMediaRouteProvider(mojom::MediaRouteProviderId provider_id,
                                  mojo::PendingRemote<mojom::MediaRouteProvider>
                                      media_route_provider_remote) override;
  void OnSinksReceived(mojom::MediaRouteProviderId provider_id,
                       const std::string& media_source,
                       const std::vector<MediaSinkInternal>& internal_sinks,
                       const std::vector<url::Origin>& origins) override;
  void GetMediaSinkServiceStatus(
      mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) override;

  // Initializes MRPs and adds them to |media_route_providers_|.
  void InitializeMediaRouteProviders();

  // Helper methods for InitializeMediaRouteProviders().
  void InitializeWiredDisplayMediaRouteProvider();
  void InitializeCastMediaRouteProvider();
  void InitializeDialMediaRouteProvider();

#if defined(OS_WIN)
  // Ensures that mDNS discovery is enabled in the Cast MRP. This can be
  // called many times but the MRPM will only be called once per registration
  // period.
  void EnsureMdnsDiscoveryEnabled();

  // Callback used to enable mDNS in the MRPM if a firewall prompt will not be
  // triggered. If a firewall prompt would be triggered, enabling mDNS won't
  // happen until the user is clearly interacting with MR.
  void OnFirewallCheckComplete(bool firewall_can_use_local_ports);
#endif

  // Gets the per-profile Cast SDK hash token used by Cast and DIAL MRPs.
  std::string GetHashToken();

  // MediaRouteProvider for casting to local screens.
  std::unique_ptr<WiredDisplayMediaRouteProvider> wired_display_provider_;

  // MediaRouteProvider for casting to Cast devices.
  std::unique_ptr<CastMediaRouteProvider, base::OnTaskRunnerDeleter>
      cast_provider_;

  // MediaRouteProvider for DIAL.
  std::unique_ptr<DialMediaRouteProvider, base::OnTaskRunnerDeleter>
      dial_provider_;

  raw_ptr<DualMediaSinkService> media_sink_service_;
  base::CallbackListSubscription media_sink_service_subscription_;

  // A status object that keeps track of sinks discovered by media sink
  // services.
  MediaSinkServiceStatus media_sink_service_status_;

  base::WeakPtrFactory<MediaRouterDesktop> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_
