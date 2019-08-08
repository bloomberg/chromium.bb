// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_EXPERIMENT_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_EXPERIMENT_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "media/base/video_codecs.h"

namespace content {

// Keeps track of all media players across all pages, and notifies them when
// they enter or leave an active experiment.
class CONTENT_EXPORT MediaExperimentManager {
 public:
  // The Client interface allows the manager to send messages back to the
  // player about experiment state.
  class CONTENT_EXPORT Client {
   public:
    Client() = default;
    virtual ~Client() = default;

    // Called when |player| becomes the focus of some experiment.
    // TODO: Should include an ExperimentId, so the player knows what to do.
    virtual void OnExperimentStarted(const MediaPlayerId& player) = 0;

    // Called when |player| stops being the focus of some experiment.  Not
    // called when the player is destroyed, or the player's client is destroyed,
    // even though it does quit being the focus at that point too.
    // TODO: Should include an ExperimentId, so the player knows what to do.
    virtual void OnExperimentStopped(const MediaPlayerId& player) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  // State for one media player.
  struct PlayerState {
    Client* client = nullptr;
    bool is_playing = false;
    bool is_fullscreen = false;
    bool is_pip = false;
  };

  class CONTENT_EXPORT ScopedPlayerState {
   public:
    ScopedPlayerState(base::OnceClosure destruction_cb, PlayerState* state);
    ScopedPlayerState(ScopedPlayerState&&);
    ~ScopedPlayerState();

    PlayerState* operator->() { return state_; }

   private:
    PlayerState* state_;
    base::OnceClosure destruction_cb_;
    DISALLOW_COPY_AND_ASSIGN(ScopedPlayerState);
  };

  MediaExperimentManager();
  virtual ~MediaExperimentManager();

  static MediaExperimentManager* Instance();

  // Notifies us that |player| has been created, and is being managed by
  // |client|.  |client| must exist until all its players have been destroyed
  // via calls to DestroyPlayer, or the client calls ClientDestroyed().
  virtual void PlayerCreated(const MediaPlayerId& player, Client* client);

  // Called when the given player has been destroyed.
  virtual void PlayerDestroyed(const MediaPlayerId& player);

  // Notify us that |client| is being destroyed.  All players that it created
  // will be deleted.  No further notifications will be sent to it.  This is
  // useful, for example, when a page is being destroyed so that we don't keep
  // sending notifications while everything is being torn down.  It's not an
  // error if |client| has no active players.
  virtual void ClientDestroyed(Client* client);

  // Update the player state.  When the returned ScopedMediaPlayerState is
  // destroyed, we will process the changes.  One may not create or destroy
  // players while the ScopedMediaPlayerState exists.
  virtual ScopedPlayerState GetPlayerState(const MediaPlayerId& player);

  // Return the number of players total.
  size_t GetPlayerCountForTesting() const;

 private:
  // Erase all players in |player_ids|.  Does not send any notifications, nor
  // does it FindRunningExperiments.
  void ErasePlayersInternal(const std::set<MediaPlayerId>& player_ids);

  // Set of all players that we know about.
  std::map<MediaPlayerId, PlayerState> players_;

  // [client] == set of all player ids that it owns.
  std::map<Client*, std::set<MediaPlayerId>> player_ids_by_client_;

  DISALLOW_COPY_AND_ASSIGN(MediaExperimentManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_EXPERIMENT_MANAGER_H_
