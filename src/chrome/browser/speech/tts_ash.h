// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_ASH_H_
#define CHROME_BROWSER_SPEECH_TTS_ASH_H_

#include <vector>
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/tts_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class ProfileManager;

namespace crosapi {

// Implements tts interface to allow Lacros to call ash to handle TTS requests,
// , manages remote TtsClient objects registered by Lacros, and caches the
// voices from Lacros.
class TtsAsh : public mojom::Tts,
               public content::VoicesChangedDelegate,
               public ProfileManagerObserver {
 public:
  explicit TtsAsh(ProfileManager* profile_manager);
  TtsAsh(const TtsAsh&) = delete;
  TtsAsh& operator=(const TtsAsh&) = delete;
  ~TtsAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Tts> pending_receiver);

  // Returns true if there is any registered Tts Client.
  bool HasTtsClient() const;

  // Returns the browser context id for primary profile.
  base::UnguessableToken GetPrimaryProfileBrowserContextId() const;

  // Returns the cached lacros voices in |out_voices| for |browser_context_id|.
  void GetCrosapiVoices(base::UnguessableToken browser_context_id,
                        std::vector<content::VoiceData>* out_voices);

  // crosapi::mojom::Tts:
  void RegisterTtsClient(mojo::PendingRemote<mojom::TtsClient> client,
                         const base::UnguessableToken& browser_context_id,
                         bool from_primary_profile) override;
  void VoicesChanged(const base::UnguessableToken& browser_context_id,
                     std::vector<mojom::TtsVoicePtr> lacros_voices) override;

 private:
  // content::VoicesChangedDelegate:
  void OnVoicesChanged() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Called when a Tts client is disconnected.
  void TtsClientDisconnected(const base::UnguessableToken& browser_context_id);

  // Owned by g_browser_process.
  ProfileManager* profile_manager_;

  // Support any number of connections.
  mojo::ReceiverSet<mojom::Tts> receivers_;

  // Registered TtsClients from Lacros by browser_context_id.
  std::map<base::UnguessableToken, mojo::Remote<mojom::TtsClient>> tts_clients_;

  // Cached lacros voices by browser_context_id.
  std::map<base::UnguessableToken, std::vector<content::VoiceData>>
      crosapi_voices_;

  base::UnguessableToken primary_profile_browser_context_id_;

  base::ScopedObservation<content::TtsController,
                          content::VoicesChangedDelegate,
                          &content::TtsController::AddVoicesChangedDelegate,
                          &content::TtsController::RemoveVoicesChangedDelegate>
      voices_changed_observation_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<TtsAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_SPEECH_TTS_ASH_H_
