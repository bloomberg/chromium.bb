// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/router/presentation/web_contents_presentation_manager.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_message_center/media_notification_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_message_center {
class MediaSessionNotificationItem;
}  // namespace media_message_center

class MediaDialogDelegate;
class MediaNotificationContainerImpl;
class MediaNotificationServiceObserver;

class MediaNotificationService
    : public KeyedService,
      public media_session::mojom::AudioFocusObserver,
      public media_message_center::MediaNotificationController,
      public MediaNotificationContainerObserver {
 public:
  explicit MediaNotificationService(Profile* profile);
  MediaNotificationService(const MediaNotificationService&) = delete;
  MediaNotificationService& operator=(const MediaNotificationService&) = delete;
  ~MediaNotificationService() override;

  void AddObserver(MediaNotificationServiceObserver* observer);
  void RemoveObserver(MediaNotificationServiceObserver* observer);

  // media_session::mojom::AudioFocusObserver implementation.
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // media_message_center::MediaNotificationController implementation.
  void ShowNotification(const std::string& id) override;
  void HideNotification(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
  void LogMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) override;

  // MediaNotificationContainerObserver implementation.
  void OnContainerExpanded(bool expanded) override {}
  void OnContainerMetadataChanged() override {}
  void OnContainerActionsChanged() override {}
  void OnContainerClicked(const std::string& id) override;
  void OnContainerDismissed(const std::string& id) override;
  void OnContainerDestroyed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Called by the |overlay_media_notifications_manager_| when an overlay
  // notification is closed.
  void OnOverlayNotificationClosed(const std::string& id);

  void OnCastNotificationsChanged();

  void SetDialogDelegate(MediaDialogDelegate* delegate);

  // True if there are active non-frozen media session notifications or active
  // cast notifications.
  bool HasActiveNotifications() const;

  // True if there are active frozen media session notifications.
  bool HasFrozenNotifications() const;

  // True if there is an open MediaDialogView associated with this service.
  bool HasOpenDialog() const;

  // Called by a |MediaNotificationService::Session| when it becomes active.
  void OnSessionBecameActive(const std::string& id);

  // Called by a |MediaNotificationService::Session| when it becomes inactive.
  void OnSessionBecameInactive(const std::string& id);

 private:
  friend class MediaNotificationServiceTest;
  friend class MediaToolbarButtonControllerTest;
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HideAfterTimeoutAndActiveAgainOnPlay);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           SessionIsRemovedImmediatelyWhenATabCloses);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest, DismissesMediaSession);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidesInactiveNotifications);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidingNotification_FeatureDisabled);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class GlobalMediaControlsDismissReason {
    kUserDismissedNotification = 0,
    kInactiveTimeout = 1,
    kTabClosed = 2,
    kMediaSessionStopped = 3,
    kMaxValue = kMediaSessionStopped,
  };

  class Session
      : public content::WebContentsObserver,
        public media_session::mojom::MediaControllerObserver,
        public media_router::WebContentsPresentationManager::Observer {
   public:
    Session(MediaNotificationService* owner,
            const std::string& id,
            std::unique_ptr<media_message_center::MediaSessionNotificationItem>
                item,
            content::WebContents* web_contents,
            mojo::Remote<media_session::mojom::MediaController> controller);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() override;

    // content::WebContentsObserver:
    void WebContentsDestroyed() override;

    // media_session::mojom::MediaControllerObserver:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr session_info) override;
    void MediaSessionMetadataChanged(
        const base::Optional<media_session::MediaMetadata>& metadata) override {
    }
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& actions)
        override {}
    void MediaSessionChanged(
        const base::Optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const base::Optional<media_session::MediaPosition>& position) override;

    // media_router::WebContentsPresentationManager::Observer:
    void OnMediaRoutesChanged(
        const std::vector<media_router::MediaRoute>& routes) override;

    media_message_center::MediaSessionNotificationItem* item() {
      return item_.get();
    }

    // Called when a new MediaController is given to the item. We need to
    // observe the same session as our underlying item.
    void SetController(
        mojo::Remote<media_session::mojom::MediaController> controller);

    // Sets the reason why this session was dismissed/removed. Can only be
    // called if the value has not already been set.
    void set_dismiss_reason(GlobalMediaControlsDismissReason reason);

    // Called when a session is interacted with (to reset |inactive_timer_|).
    void OnSessionInteractedWith();

    // Called when the notification associated with this session is pulled out
    // into an overlay or it's overlay is closed.
    void OnSessionOverlayStateChanged(bool is_in_overlay);

    bool IsPlaying();

   private:
    static void RecordDismissReason(GlobalMediaControlsDismissReason reason);

    void StartInactiveTimer();

    void OnInactiveTimerFired();

    void RecordInteractionDelayAfterPause();

    void MarkActiveIfNecessary();

    MediaNotificationService* owner_;
    const std::string id_;
    std::unique_ptr<media_message_center::MediaSessionNotificationItem> item_;

    // Used to stop/hide a paused session after a period of inactivity.
    base::OneShotTimer inactive_timer_;

    base::TimeTicks last_interaction_time_ = base::TimeTicks::Now();

    // The reason why this session was dismissed/removed.
    base::Optional<GlobalMediaControlsDismissReason> dismiss_reason_;

    // True if the session's playback state is "playing".
    bool is_playing_ = false;

    // True if we're currently marked inactive.
    bool is_marked_inactive_ = false;

    // True if we're in an overlay notification.
    bool is_in_overlay_ = false;

    // Used to receive updates to the Media Session playback state.
    mojo::Receiver<media_session::mojom::MediaControllerObserver>
        observer_receiver_{this};

    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager_;
  };

  void OnItemUnfrozen(const std::string& id);

  void OnReceivedAudioFocusRequests(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions);

  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id);

  MediaDialogDelegate* dialog_delegate_ = nullptr;

  OverlayMediaNotificationsManager overlay_media_notifications_manager_;

  // Used to track whether there are any active controllable sessions. If not,
  // then there's nothing to show in the dialog and we can hide the toolbar
  // icon. Contains sessions from both Media Session API and Cast.
  std::unordered_set<std::string> active_controllable_session_ids_;

  // Tracks the sessions that are currently frozen. If there are only frozen
  // sessions, we will disable the toolbar icon and wait to hide it.
  std::unordered_set<std::string> frozen_session_ids_;

  // Tracks the sessions that are currently dragged out of the dialog. These
  // should not be shown in the dialog and will be ignored for showing the
  // toolbar icon.
  std::unordered_set<std::string> dragged_out_session_ids_;

  // Tracks the sessions that are currently inactive. Sessions become inactive
  // after a period of time of being paused with no user interaction. Inactive
  // sessions are hidden from the dialog until the user interacts with them
  // again (e.g. by playing the session).
  std::unordered_set<std::string> inactive_session_ids_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<std::string, Session> sessions_;

  // A map of all containers we're currently observing.
  std::map<std::string, MediaNotificationContainerImpl*> observed_containers_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  std::unique_ptr<CastMediaNotificationProvider> cast_notification_provider_;

  base::ObserverList<MediaNotificationServiceObserver> observers_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
