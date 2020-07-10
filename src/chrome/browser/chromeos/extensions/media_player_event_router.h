// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_

#include "base/macros.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Event router class for events related to Mediaplayer.
class MediaPlayerEventRouter {
 public:
  explicit MediaPlayerEventRouter(content::BrowserContext* context);
  virtual ~MediaPlayerEventRouter();

  // Send notification that next-track shortcut key was pressed.
  void NotifyNextTrack();

 // Send notification that previous-track shortcut key was pressed.
  void NotifyPrevTrack();

  // Send notification that play/pause shortcut key was pressed.
  void NotifyTogglePlayState();

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
