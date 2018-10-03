// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
#define SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_

#include <list>
#include <unordered_map>

#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

class AudioFocusManager {
 public:
  using RequestId = int;
  using RequestResponse = std::pair<RequestId, bool>;

  // Returns Chromium's internal AudioFocusManager.
  static AudioFocusManager* GetInstance();

  // Requests audio focus with |type| for the |media_session| with
  // |session_info|. The function will return a |RequestResponse| which contains
  // a |RequestId| and a boolean as to whether the request was successful. If a
  // session wishes to request a different focus type it should provide its
  // previous request id as |previous_id|.
  RequestResponse RequestAudioFocus(mojom::MediaSessionPtr media_session,
                                    mojom::MediaSessionInfoPtr session_info,
                                    mojom::AudioFocusType type,
                                    base::Optional<RequestId> previous_id);

  // Abandons audio focus for a request with |id|. The next request on top of
  // the stack will be granted audio focus.
  void AbandonAudioFocus(RequestId id);

  // Returns the current audio focus type for a request with |id|.
  mojom::AudioFocusType GetFocusTypeForSession(RequestId id);

  // Adds/removes audio focus observers.
  mojo::InterfacePtrSetElementId AddObserver(mojom::AudioFocusObserverPtr);
  void RemoveObserver(mojo::InterfacePtrSetElementId);

 private:
  friend struct base::DefaultSingletonTraits<AudioFocusManager>;
  friend class AudioFocusManagerTest;

  // Media internals UI needs access to internal state.
  friend class MediaInternalsAudioFocusTest;
  friend class MediaInternals;

  // Flush for testing will flush any pending messages to the observers.
  void FlushForTesting();

  // Reset for testing will clear any built up internal state.
  void ResetForTesting();

  AudioFocusManager();
  ~AudioFocusManager();

  void RemoveFocusEntryIfPresent(RequestId id);

  // Weak reference of managed observers. Observers are expected to remove
  // themselves before being destroyed.
  mojo::InterfacePtrSet<mojom::AudioFocusObserver> observers_;

  // StackRow is a MediaSessionObserver and holds a cached copy of the latest
  // MediaSessionInfo associated with the MediaSession. By keeping the info
  // cached and readily available we can make audio focus decisions quickly
  // without waiting on MediaSessions.
  class StackRow : public mojom::MediaSessionObserver {
   public:
    StackRow(mojom::MediaSessionPtr session,
             mojom::MediaSessionInfoPtr current_info,
             mojom::AudioFocusType audio_focus_type,
             RequestId id);
    ~StackRow() override;

    // mojom::MediaSessionObserver.
    void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) override;

    mojom::MediaSession* session();
    const mojom::MediaSessionInfoPtr& info() const;
    mojom::AudioFocusType audio_focus_type() const { return audio_focus_type_; }

    bool IsActive() const;
    int id() { return id_; }

    // Flush any pending mojo messages for testing.
    void FlushForTesting();

   private:
    void OnConnectionError();

    int id_;
    mojom::MediaSessionPtr session_;
    mojom::MediaSessionInfoPtr current_info_;
    mojom::AudioFocusType audio_focus_type_;
    mojo::Binding<mojom::MediaSessionObserver> binding_;

    DISALLOW_COPY_AND_ASSIGN(StackRow);
  };

  // A stack of Mojo interface pointers and their requested audio focus type.
  // A MediaSession must abandon audio focus before its destruction.
  std::list<std::unique_ptr<StackRow>> audio_focus_stack_;

  // Adding observers should happen on the same thread that the service is
  // running on.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManager);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
