// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_BASE_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_BASE_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/common/media_route.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

#if !defined(OS_ANDROID)
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#endif  // !defined(OS_ANDROID)

namespace media_router {

class MediaRouterBase : public MediaRouter {
 public:
  MediaRouterBase(const MediaRouterBase&) = delete;
  MediaRouterBase& operator=(const MediaRouterBase&) = delete;

  ~MediaRouterBase() override;

  // Must be called before invoking any other method.
  void Initialize();

  // MediaRouter implementation.
  base::CallbackListSubscription AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override;
  void OnIncognitoProfileShutdown() override;
  IssueManager* GetIssueManager() final;
  std::vector<MediaRoute> GetCurrentRoutes() const override;
  std::unique_ptr<media::FlingingController> GetFlingingController(
      const MediaRoute::Id& route_id) override;
#if !defined(OS_ANDROID)
  void GetMediaController(
      const MediaRoute::Id& route_id,
      mojo::PendingReceiver<mojom::MediaController> controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;
  base::Value GetLogs() const override;
#endif  // !defined(OS_ANDROID)
  base::Value GetState() const override;
  void GetProviderState(
      mojom::MediaRouteProviderId provider_id,
      mojom::MediaRouteProvider::GetStateCallback callback) const override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterBaseTest, CreatePresentationIds);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterBaseTest, NotifyCallbacks);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           ListenForConnnectionStateChange);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest, GetMediaRoutes);

  MediaRouterBase();

  // Generates a unique presentation id. Shared between Android and desktop.
  static std::string CreatePresentationId();

  void NotifyPresentationConnectionStateChange(
      const MediaRoute::Id& route_id,
      blink::mojom::PresentationConnectionState state);
  void NotifyPresentationConnectionClose(
      const MediaRoute::Id& route_id,
      blink::mojom::PresentationConnectionCloseReason reason,
      const std::string& message);

  // Returns true when there is at least one MediaRoute that can be returned by
  // JoinRoute().
  bool HasJoinableRoute() const;

  // Returns a pointer to the MediaRoute whose ID is |route_id|, or nullptr
  // if not found.
  const MediaRoute* GetRoute(const MediaRoute::Id& route_id) const;

  using PresentationConnectionStateChangedCallbacks =
      base::RepeatingCallbackList<void(
          const content::PresentationConnectionStateChangeInfo&)>;

  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationConnectionStateChangedCallbacks>>
      presentation_connection_state_callbacks_;

 private:
  friend class MediaRouterBaseTest;
  friend class MediaRouterMojoTest;

  class InternalMediaRoutesObserver;

  // Called when a PresentationConnectionStateChangedCallback associated with
  // |route_id| is removed from |presentation_connection_state_callbacks_|.
  void OnPresentationConnectionStateCallbackRemoved(
      const MediaRoute::Id& route_id);

  // KeyedService
  void Shutdown() override;

  IssueManager issue_manager_;

  std::unique_ptr<InternalMediaRoutesObserver> internal_routes_observer_;
  bool initialized_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_BASE_H_
