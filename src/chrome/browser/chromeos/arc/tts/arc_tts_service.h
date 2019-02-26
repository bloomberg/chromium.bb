// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/common/tts.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
class TtsControllerDelegate;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Provides text to speech services and events to Chrome OS via Android's text
// to speech API.
class ArcTtsService : public KeyedService,
                      public mojom::TtsHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcTtsService* GetForBrowserContext(content::BrowserContext* context);
  static ArcTtsService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcTtsService(content::BrowserContext* context,
                ArcBridgeService* bridge_service);
  ~ArcTtsService() override;

  // mojom::TtsHost overrides:
  void OnTtsEvent(uint32_t id,
                  mojom::TtsEventType event_type,
                  uint32_t char_index,
                  const std::string& error_msg) override;

  void set_tts_controller_delegate_for_testing(
      content::TtsControllerDelegate* tts_controller_delegate) {
    tts_controller_delegate_ = tts_controller_delegate;
  }

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  content::TtsControllerDelegate* tts_controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ArcTtsService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_
