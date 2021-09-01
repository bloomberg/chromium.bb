// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"
#include "url/origin.h"

namespace media_router {

class CastActivityFactoryForTest;
class AppActivity;
class CastSession;
class MediaSinkServiceBase;

// Base class for CastActivityManager including only functionality needed by
// AppActivity.
class CastActivityManagerBase {
 public:
  CastActivityManagerBase() = default;
  CastActivityManagerBase(const CastActivityManagerBase&) = delete;
  CastActivityManagerBase& operator=(const CastActivityManagerBase&) = delete;

  virtual cast_channel::ResultCallback MakeResultCallbackForRoute(
      const std::string& route_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) = 0;

 protected:
  // The destructor is protected to allow deletion only through a pointer to a
  // derived type.
  ~CastActivityManagerBase() = default;
};

// Handles launching and terminating Cast application on a Cast receiver, and
// acts as the source of truth for Cast activities and MediaRoutes.
class CastActivityManager : public CastActivityManagerBase,
                            public cast_channel::CastMessageHandler::Observer,
                            public CastSessionTracker::Observer {
 public:
  // |media_sink_service|: Provides Cast MediaSinks.
  // |message_handler|: Used for sending and receiving messages to Cast
  // receivers.
  // |media_router|: Mojo ptr to MediaRouter.
  // |hash_token|: Used for hashing receiver IDs in messages sent to the Cast
  // SDK.
  CastActivityManager(MediaSinkServiceBase* media_sink_service,
                      CastSessionTracker* session_tracker,
                      cast_channel::CastMessageHandler* message_handler,
                      mojom::MediaRouter* media_router,
                      mojom::Logger* logger,
                      const std::string& hash_token);
  ~CastActivityManager() override;

  // Adds or removes a route query with |source|. When adding a route query, if
  // the current list of routes is non-empty, the query will be immediately
  // updated with the current list.
  // TODO(https://crbug.com/882481): Simplify the route query API.
  void AddRouteQuery(const MediaSource::Id& source);
  void RemoveRouteQuery(const MediaSource::Id& source);

  // Launches a Cast session with parameters given by |cast_source| to |sink|.
  // Returns the created MediaRoute and notifies existing route queries.
  void LaunchSession(const CastMediaSource& cast_source,
                     const MediaSinkInternal& sink,
                     const std::string& presentation_id,
                     const url::Origin& origin,
                     int tab_id,
                     bool incognito,
                     mojom::MediaRouteProvider::CreateRouteCallback callback);

  void JoinSession(const CastMediaSource& cast_source,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int tab_id,
                   bool incognito,
                   mojom::MediaRouteProvider::JoinRouteCallback callback);

  // Terminates a Cast session represented by |route_id|.
  void TerminateSession(
      const MediaRoute::Id& route_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback);

  bool CreateMediaController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer);

  const MediaRoute* GetRoute(const MediaRoute::Id& route_id) const;
  std::vector<MediaRoute> GetRoutes() const;
  CastSessionTracker* GetCastSessionTracker() const { return session_tracker_; }

  // cast_channel::CastMessageHandler::Observer overrides.
  void OnAppMessage(int channel_id,
                    const cast::channel::CastMessage& message) override;
  void OnInternalMessage(int channel_id,
                         const cast_channel::InternalMessage& message) override;

  // CastSessionTracker::Observer implementation.
  void OnSessionAddedOrUpdated(const MediaSinkInternal& sink,
                               const CastSession& session) override;
  void OnSessionRemoved(const MediaSinkInternal& sink) override;
  void OnMediaStatusUpdated(const MediaSinkInternal& sink,
                            const base::Value& media_status,
                            absl::optional<int> request_id) override;

  static void SetActitityFactoryForTest(CastActivityFactoryForTest* factory) {
    cast_activity_factory_for_test_ = factory;
  }

  cast_channel::ResultCallback MakeResultCallbackForRoute(
      const std::string& route_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) override;

  const MediaRoute* FindMirroringRouteForTab(int32_t tab_id);

  void SendRouteMessage(const std::string& media_route_id,
                        const std::string& message);

 private:
  friend class CastActivityManagerTest;
  FRIEND_TEST_ALL_PREFIXES(CastActivityManagerTest,
                           LaunchSessionTerminatesExistingSessionOnSink);
  FRIEND_TEST_ALL_PREFIXES(CastActivityManagerTest,
                           LaunchSessionTerminatesExistingSessionFromTab);
  FRIEND_TEST_ALL_PREFIXES(CastActivityManagerTest, SendMediaRequestToReceiver);

  using ActivityMap =
      base::flat_map<MediaRoute::Id, std::unique_ptr<CastActivity>>;
  using AppActivityMap = base::flat_map<MediaRoute::Id, AppActivity*>;

  void SendRouteJsonMessage(const std::string& media_route_id,
                            const std::string& message,
                            data_decoder::DataDecoder::ValueOrError result);

  void LaunchSessionParsed(
      const CastMediaSource& cast_source,
      const MediaSinkInternal& sink,
      const std::string& presentation_id,
      const url::Origin& origin,
      int tab_id,
      bool incognito,
      mojom::MediaRouteProvider::CreateRouteCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  // Bundle of parameters for DoLaunchSession().
  struct DoLaunchSessionParams {
    // Note: The compiler-generated constructors and destructor would be
    // sufficient here, but the style guide requires them to be defined
    // explicitly.
    DoLaunchSessionParams(
        const MediaRoute& route,
        const CastMediaSource& cast_source,
        const MediaSinkInternal& sink,
        const url::Origin& origin,
        int tab_id,
        const absl::optional<base::Value> app_params,
        mojom::MediaRouteProvider::CreateRouteCallback callback);
    DoLaunchSessionParams(const DoLaunchSessionParams& other) = delete;
    DoLaunchSessionParams(DoLaunchSessionParams&& other);
    ~DoLaunchSessionParams();
    DoLaunchSessionParams& operator=(DoLaunchSessionParams&) = delete;
    DoLaunchSessionParams& operator=(DoLaunchSessionParams&&) = default;

    // The route for which a session is being launched.
    MediaRoute route;

    // The media source for which a session is being launched.
    CastMediaSource cast_source;

    // The sink for which a session is being launched.
    MediaSinkInternal sink;

    // The origin of the Cast SDK client. Used for auto-join.
    url::Origin origin;

    // The tab ID of the Cast SDK client. Used for auto-join.
    int tab_id;

    // The JSON object sent from the Cast SDK.
    absl::optional<base::Value> app_params;

    // Callback to execute after the launch request has been sent.
    mojom::MediaRouteProvider::CreateRouteCallback callback;
  };

  void DoLaunchSession(DoLaunchSessionParams params);
  void SetPendingLaunch(DoLaunchSessionParams params);
  void OnActivityStopped(const std::string& route_id);

  // Removes an activity, terminating any associated connections, then
  // notifies the media router that routes have been updated.
  void RemoveActivity(
      ActivityMap::iterator activity_it,
      blink::mojom::PresentationConnectionState state,
      blink::mojom::PresentationConnectionCloseReason close_reason);

  // Removes an activity without sending the usual notification.
  //
  // TODO(jrw): Figure out why it's desirable to avoid sending the usual
  // notification sometimes.
  void RemoveActivityWithoutNotification(
      ActivityMap::iterator activity_it,
      blink::mojom::PresentationConnectionState state,
      blink::mojom::PresentationConnectionCloseReason close_reason);

  void NotifyAllOnRoutesUpdated();
  void NotifyOnRoutesUpdated(const MediaSource::Id& source_id,
                             const std::vector<MediaRoute>& routes);

  void HandleLaunchSessionResponse(
      DoLaunchSessionParams params,
      cast_channel::LaunchSessionResponse response);
  void HandleStopSessionResponse(
      const MediaRoute::Id& route_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback,
      cast_channel::Result result);
  void HandleLaunchSessionResponseFailures(
      ActivityMap::iterator activity_it,
      DoLaunchSessionParams params,
      const std::string& message,
      RouteRequestResult::ResultCode result_code);
  void EnsureConnection(const std::string& client_id,
                        int channel_id,
                        const std::string& destination_id,
                        const CastMediaSource& cast_source);

  AppActivity* FindActivityForAutoJoin(const CastMediaSource& cast_source,
                                       const url::Origin& origin,
                                       int tab_id);
  bool CanJoinSession(const AppActivity& activity,
                      const CastMediaSource& cast_source,
                      bool incognito) const;
  AppActivity* FindActivityForSessionJoin(const CastMediaSource& cast_source,
                                          const std::string& presentation_id);

  // Creates and stores a AppActivity representing a non-local
  // activity.
  void AddNonLocalActivity(const MediaSinkInternal& sink,
                           const CastSession& session);

  void SendFailedToCastIssue(const MediaSink::Id& sink_id,
                             const MediaRoute::Id& route_id);

  // These methods return |activities_.end()| when nothing is found.
  ActivityMap::iterator FindActivityByChannelId(int channel_id);
  ActivityMap::iterator FindActivityBySink(const MediaSinkInternal& sink);

  AppActivity* AddAppActivity(const MediaRoute& route,
                              const std::string& app_id);
  CastActivity* AddMirroringActivity(const MediaRoute& route,
                                     const std::string& app_id,
                                     int tab_id,
                                     const CastSinkExtraData& cast_data);

  // Returns a sink used to convert a mirroring activity to a cast activity.
  // If no conversion should occur, returns absl::nullopt.
  absl::optional<MediaSinkInternal> ConvertMirrorToCast(int tab_id);

  std::string ChooseAppId(const CastMediaSource& source,
                          const MediaSinkInternal& sink) const;

  void TerminateAllLocalMirroringActivities();

  static CastActivityFactoryForTest* cast_activity_factory_for_test_;

  base::flat_set<MediaSource::Id> route_queries_;

  // This map contains all activities--both presentation activities and
  // mirroring activities.
  ActivityMap activities_;

  // The values of this map are the subset of those in |activites_| where
  // there is a AppActivity.
  AppActivityMap app_activities_;

  // Mapping from tab IDs to the active route for that tab.  This map is used to
  // ensure that there is at most one active route for each tab.  Removing this
  // map and the code that uses it will allow a tab to be cast to multiple
  // receivers, but there may be unintended consequences, such as confusing
  // users or causing performance problems on low-end devices.
  base::flat_map<int, MediaRoute::Id> routes_by_tab_;

  // Information for a session that will be launched once |this| is notified
  // that the existing session on the receiver has been removed. We only store
  // one pending launch at a time so that we don't accumulate orphaned pending
  // launches over time.
  absl::optional<DoLaunchSessionParams> pending_launch_;

  // The following raw pointer fields are assumed to outlive |this|.
  MediaSinkServiceBase* const media_sink_service_;
  CastSessionTracker* const session_tracker_;
  cast_channel::CastMessageHandler* const message_handler_;
  mojom::MediaRouter* const media_router_;
  mojom::Logger* const logger_;

  const std::string hash_token_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastActivityManager> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_
