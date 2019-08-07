// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

class MediaWebContentsObserver;
class PictureInPictureSession;
class WebContents;
class WebContentsImpl;

// TODO(thakis,mlamouri): PictureInPictureWindowControllerImpl isn't
// CONTENT_EXPORT'd because it creates complicated build issues with
// WebContentsUserData being a non-exported template. As a result, the class
// uses CONTENT_EXPORT for methods that are being used from tests.
// CONTENT_EXPORT should be moved back to the class when the Windows build will
// work with it. https://crbug.com/589840.
class PictureInPictureWindowControllerImpl
    : public PictureInPictureWindowController,
      public WebContentsUserData<PictureInPictureWindowControllerImpl>,
      public WebContentsObserver {
 public:
  // Gets a reference to the controller associated with |initiator| and creates
  // one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  CONTENT_EXPORT static PictureInPictureWindowControllerImpl*
  GetOrCreateForWebContents(WebContents* initiator);

  ~PictureInPictureWindowControllerImpl() override;

  using PlayerSet = std::set<int>;
  using MutedMediaPlayerMap = std::map<RenderFrameHost*, PlayerSet>;

  // PictureInPictureWindowController:
  CONTENT_EXPORT gfx::Size Show() override;
  CONTENT_EXPORT void Close(bool should_pause_video) override;
  CONTENT_EXPORT void CloseAndFocusInitiator() override;
  CONTENT_EXPORT void OnWindowDestroyed() override;
  CONTENT_EXPORT void EmbedSurface(const viz::SurfaceId& surface_id,
                                   const gfx::Size& natural_size) override;
  CONTENT_EXPORT OverlayWindow* GetWindowForTesting() override;
  CONTENT_EXPORT void UpdateLayerBounds() override;
  CONTENT_EXPORT bool IsPlayerActive() override;
  CONTENT_EXPORT bool IsPlayerMuted() override;
  CONTENT_EXPORT WebContents* GetInitiatorWebContents() override;
  CONTENT_EXPORT bool TogglePlayPause() override;
  CONTENT_EXPORT bool ToggleMute() override;
  CONTENT_EXPORT void UpdatePlaybackState(bool is_playing,
                                          bool reached_end_of_stream) override;
  CONTENT_EXPORT void UpdateMutedState() override;
  CONTENT_EXPORT void SetAlwaysHidePlayPauseButton(bool is_visible) override;
  CONTENT_EXPORT void SetAlwaysHideMuteButton(bool is_visible) override;
  CONTENT_EXPORT void SkipAd() override;
  CONTENT_EXPORT void NextTrack() override;
  CONTENT_EXPORT void PreviousTrack() override;

  CONTENT_EXPORT void MediaSessionActionsChanged(
      const std::set<media_session::mojom::MediaSessionAction>& actions);

  // WebContentsObserver:
  void MediaStartedPlaying(const MediaPlayerInfo&,
                           const MediaPlayerId&) override;
  void MediaStoppedPlaying(const MediaPlayerInfo&,
                           const MediaPlayerId&,
                           WebContentsObserver::MediaStoppedReason) override;
  void MediaMutedStatusChanged(const MediaPlayerId&, bool muted) override;

  // TODO(mlamouri): temporary method used because of the media player id is
  // stored in a different location from the one that is used to update the
  // state of this object.
  void UpdateMediaPlayerId();

  // Sets the active Picture-in-Picture session associated with the controller.
  // This is different from the service's active session as there is one
  // controller per WebContents and one service per RenderFrameHost.
  // The current session may be shut down as a side effect of this.
  void SetActiveSession(PictureInPictureSession* session);

 private:
  friend class WebContentsUserData<PictureInPictureWindowControllerImpl>;

  // Use PictureInPictureWindowControllerImpl::GetOrCreateForWebContents() to
  // create an instance.
  CONTENT_EXPORT explicit PictureInPictureWindowControllerImpl(
      WebContents* initiator);

  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  void OnLeavingPictureInPicture(bool should_pause_video);

  // Internal method to set the states after the window was closed, whether via
  // the system or Chromium.
  void CloseInternal(bool should_pause_video);

  // Creates a new window if the previous one was destroyed. It can happen
  // because of the system control of the window.
  void EnsureWindow();

  // Allow play/pause button to be visible if Media Session actions "play" and
  // "pause" are both handled by the website or if
  // always_hide_play_pause_button_ is false.
  void UpdatePlayPauseButtonVisibility();

  // Used to add/remove a muted player entry to |muted_players_|.
  void AddMutedPlayerEntry(const MediaPlayerId& id);
  bool RemoveMutedPlayerEntry(const MediaPlayerId& id);

  std::unique_ptr<OverlayWindow> window_;

  // TODO(929156): remove this as it should be accessible via `web_contents()`.
  WebContentsImpl* const initiator_;

  // Used to determine the state of the media player and route messages to
  // the corresponding media player with id |media_player_id_|.
  MediaWebContentsObserver* media_web_contents_observer_;
  base::Optional<MediaPlayerId> media_player_id_;

  viz::SurfaceId surface_id_;

  // Used to track the muted state of all players.
  MutedMediaPlayerMap muted_players_;

  // Used to show/hide some actions in Picture-in-Picture window. These are set
  // to true when website handles some Media Session actions.
  bool media_session_action_play_handled_ = false;
  bool media_session_action_pause_handled_ = false;
  bool media_session_action_skip_ad_handled_ = false;
  bool media_session_action_next_track_handled_ = false;
  bool media_session_action_previous_track_handled_ = false;

  // Used to hide play/pause button if video is a MediaStream or has infinite
  // duration. Play/pause button visibility can be overridden by the Media
  // Session API in UpdatePlayPauseButtonVisibility().
  bool always_hide_play_pause_button_ = false;

  // Used to hide mute button if video has no audio track or if MuteButton
  // origin trial is disabled.
  bool always_hide_mute_button_ = false;

  // Session currently associated with the Picture-in-Picture window. The
  // session object makes the bridge with the renderer process by handling
  // requests and holding states such as the active player id.
  // The session will be nullptr when there is no active session.
  PictureInPictureSession* active_session_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
