// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"

namespace {

bool ShouldHideNotification(const raw_ptr<Profile> profile,
                            const media_router::MediaRoute& route) {
  // TODO(crbug.com/1195382): Display multizone group route.
  if (route.is_connecting()) {
    return true;
  }

  if (media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    // Hide a route if it's not for display or it's a mirroring route.
    if (route.media_source().IsTabMirroringSource() ||
        route.media_source().IsDesktopMirroringSource() ||
        route.media_source().IsLocalFileSource())
      return true;
  } else if (route.controller_type() !=
             media_router::RouteControllerType::kGeneric) {
    return true;
  }

  if (!route.media_source().IsCastPresentationUrl()) {
    return false;
  }
  std::unique_ptr<media_router::CastMediaSource> source =
      media_router::CastMediaSource::FromMediaSource(route.media_source());
  // If the session is multizone member, then it would appear as a duplicate of
  // the multizone group's session, so it should instead be hidden.
  return source && source->GetAppIds().size() == 1 &&
         base::Contains(media_router::kMultizoneMemberAppIds,
                        source->GetAppIds()[0]);
}

}  // namespace

CastMediaNotificationProducer::CastMediaNotificationProducer(
    Profile* profile,
    global_media_controls::MediaItemManager* item_manager,
    base::RepeatingClosure items_changed_callback)
    : CastMediaNotificationProducer(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile),
          item_manager,
          std::move(items_changed_callback)) {}

CastMediaNotificationProducer::CastMediaNotificationProducer(
    Profile* profile,
    media_router::MediaRouter* router,
    global_media_controls::MediaItemManager* item_manager,
    base::RepeatingClosure items_changed_callback)
    : media_router::MediaRoutesObserver(router),
      profile_(profile),
      router_(router),
      item_manager_(item_manager),
      items_changed_callback_(std::move(items_changed_callback)),
      item_ui_observer_set_(this) {}

CastMediaNotificationProducer::~CastMediaNotificationProducer() = default;

base::WeakPtr<media_message_center::MediaNotificationItem>
CastMediaNotificationProducer::GetMediaItem(const std::string& id) {
  const auto item_it = items_.find(id);
  if (item_it == items_.end())
    return nullptr;
  return item_it->second.GetWeakPtr();
}

std::set<std::string>
CastMediaNotificationProducer::GetActiveControllableItemIds() {
  std::set<std::string> ids;
  for (const auto& item : items_) {
    if (item.second.is_active())
      ids.insert(item.first);
  }
  return ids;
}

bool CastMediaNotificationProducer::HasFrozenItems() {
  return false;
}

void CastMediaNotificationProducer::OnItemShown(
    const std::string& id,
    global_media_controls::MediaItemUI* item_ui) {
  if (item_ui)
    item_ui_observer_set_.Observe(id, item_ui);
}

void CastMediaNotificationProducer::OnDialogDisplayed() {
  media_message_center::RecordConcurrentCastNotificationCount(
      GetActiveItemCount());
}

bool CastMediaNotificationProducer::IsItemActivelyPlaying(
    const std::string& id) {
  // TODO: This is a stub, since we currently only care about
  // MediaSessionNotificationProducer, but we probably should care about the
  // other ones.
  return false;
}

void CastMediaNotificationProducer::OnMediaItemUIDismissed(
    const std::string& id) {
  auto item = GetMediaItem(id);
  if (item) {
    item->Dismiss();
  }
  if (!HasActiveItems()) {
    items_changed_callback_.Run();
  }
}

void CastMediaNotificationProducer::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  const bool had_items = HasActiveItems();

  base::EraseIf(items_, [&routes](const auto& item) {
    return std::find_if(routes.begin(), routes.end(),
                        [&item](const media_router::MediaRoute& route) {
                          return item.first == route.media_route_id();
                        }) == routes.end();
  });

  for (const auto& route : routes) {
    if (ShouldHideNotification(profile_, route))
      continue;

    auto item_it =
        std::find_if(items_.begin(), items_.end(), [&route](const auto& item) {
          return item.first == route.media_route_id();
        });
    if (item_it == items_.end()) {
      mojo::Remote<media_router::mojom::MediaController> controller_remote;
      mojo::PendingReceiver<media_router::mojom::MediaController>
          controller_receiver = controller_remote.BindNewPipeAndPassReceiver();
      auto it_pair = items_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(route.media_route_id()),
          std::forward_as_tuple(route, item_manager_,
                                std::make_unique<CastMediaSessionController>(
                                    std::move(controller_remote)),
                                profile_));
      router_->GetMediaController(
          route.media_route_id(), std::move(controller_receiver),
          it_pair.first->second.GetObserverPendingRemote());
      item_manager_->ShowItem(route.media_route_id());
    } else {
      item_it->second.OnRouteUpdated(route);
    }
  }
  if (HasActiveItems() != had_items)
    items_changed_callback_.Run();
}

size_t CastMediaNotificationProducer::GetActiveItemCount() const {
  return std::count_if(items_.begin(), items_.end(), [](const auto& item) {
    return item.second.is_active();
  });
}

bool CastMediaNotificationProducer::HasActiveItems() const {
  return GetActiveItemCount() != 0;
}

bool CastMediaNotificationProducer::HasLocalMediaRoute() const {
  return std::find_if(items_.begin(), items_.end(), [](const auto& item) {
           return item.second.is_local_presentation();
         }) != items_.end();
}
