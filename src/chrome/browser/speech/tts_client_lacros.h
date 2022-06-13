// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CLIENT_LACROS_H_
#define CHROME_BROWSER_SPEECH_TTS_CLIENT_LACROS_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class BrowserContext;
}

// Implements crosapi::mojom::TtsClient, which is called by ash to handle
// TTS requests to Lacros such as retrieving voice data, etc. It also manages
// To send TTS requests ash. TtsClientLacros is created per BrowserContext.
class TtsClientLacros : public extensions::BrowserContextKeyedAPI,
                        public crosapi::mojom::TtsClient {
 public:
  explicit TtsClientLacros(content::BrowserContext* context);
  TtsClientLacros(const TtsClientLacros&) = delete;
  TtsClientLacros& operator=(const TtsClientLacros&) = delete;
  ~TtsClientLacros() override;

  // crosapi::mojom::TtsClient:
  void VoicesChanged(
      std::vector<crosapi::mojom::TtsVoicePtr> mojo_all_voices) override;

  const base::UnguessableToken& browser_context_id() const {
    return browser_context_id_;
  }

  // Returns the cached voices in |out_voices|, which are the voices available
  // for Lacros including the ones provided by both Ash and Lacros.
  void GetAllVoices(std::vector<content::VoiceData>* out_voices);

  content::BrowserContext* browser_context() { return browser_context_; }

  static TtsClientLacros* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<TtsClientLacros>;

  // Notifies Ash about Lacros voices change.
  void NotifyLacrosVoicesChanged();

  void OnGetAllVoices(std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices);

  content::BrowserContext* browser_context_;  // not owned.
  base::UnguessableToken browser_context_id_;
  mojo::Receiver<crosapi::mojom::TtsClient> receiver_{this};

  // Cached voices for |browser_context_|, including both ash and lacros voices.
  std::vector<content::VoiceData> all_voices_;

  base::WeakPtrFactory<TtsClientLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CLIENT_LACROS_H_
