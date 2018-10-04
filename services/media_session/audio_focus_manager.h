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
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

class AudioFocusManager : public mojom::AudioFocusManager,
                          public mojom::AudioFocusManagerDebug {
 public:
  using RequestId = uint64_t;

  // Returns Chromium's internal AudioFocusManager.
  static AudioFocusManager* GetInstance();

  // mojom::AudioFocusManager.
  void RequestAudioFocus(mojom::AudioFocusRequestClientRequest request,
                         mojom::MediaSessionPtr media_session,
                         mojom::MediaSessionInfoPtr session_info,
                         mojom::AudioFocusType type,
                         RequestAudioFocusCallback callback) override;
  void GetFocusRequests(GetFocusRequestsCallback callback) override;
  void AddObserver(mojom::AudioFocusObserverPtr observer) override;

  // mojom::AudioFocusManagerDebug.
  void GetDebugInfoForRequest(uint64_t request_id,
                              GetDebugInfoForRequestCallback callback) override;

  // Bind to a mojom::AudioFocusManagerRequest.
  void BindToInterface(mojom::AudioFocusManagerRequest request);

  // Bind to a mojom::AudioFocusManagerDebugRequest.
  void BindToDebugInterface(mojom::AudioFocusManagerDebugRequest request);

  // This will close all Mojo bindings and interface pointers. This should be
  // called by the MediaSession service before it is destroyed.
  void CloseAllMojoObjects();

 private:
  friend struct base::DefaultSingletonTraits<AudioFocusManager>;
  friend class AudioFocusManagerTest;

  // StackRow is an AudioFocusRequestClient and allows a media session to
  // control its audio focus.
  class StackRow;

  void RequestAudioFocusInternal(std::unique_ptr<StackRow>,
                                 mojom::AudioFocusType,
                                 base::OnceCallback<void()>);
  void EnforceAudioFocusRequest(mojom::AudioFocusType);

  void AbandonAudioFocusInternal(RequestId);
  void EnforceAudioFocusAbandon(mojom::AudioFocusType);

  // Flush for testing will flush any pending messages to the observers.
  void FlushForTesting();

  // Reset for testing will clear any built up internal state.
  void ResetForTesting();

  AudioFocusManager();
  ~AudioFocusManager() override;

  std::unique_ptr<StackRow> RemoveFocusEntryIfPresent(RequestId id);

  bool IsSessionOnTopOfAudioFocusStack(RequestId id,
                                       mojom::AudioFocusType type) const;

  // Holds mojo bindings for the Audio Focus Manager API.
  mojo::BindingSet<mojom::AudioFocusManager> bindings_;

  // Holds mojo bindings for the Audio Focus Manager Debug API.
  mojo::BindingSet<mojom::AudioFocusManagerDebug> debug_bindings_;

  // Weak reference of managed observers. Observers are expected to remove
  // themselves before being destroyed.
  mojo::InterfacePtrSet<mojom::AudioFocusObserver> observers_;

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
