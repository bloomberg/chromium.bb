// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_PLAYER_ACTION_H_
#define WEBKIT_GLUE_MEDIA_PLAYER_ACTION_H_

// Commands that can be sent to the MediaPlayer via a WebView.
struct MediaPlayerAction {
  enum CommandTypeBit {
    NONE = 0x0,
    PLAY = 0x1,
    PAUSE = 0x2,
    MUTE = 0x4,
    UNMUTE = 0x8,
    LOOP = 0x10,
    NO_LOOP = 0x20,
  };

  // A bitfield representing the actions that the context menu should execute
  // on the originating node.
  int32 command;

  MediaPlayerAction() : command(NONE) {}
  explicit MediaPlayerAction(int c) : command(c) {}
};

#endif  // WEBKIT_GLUE_MEDIA_PLAYER_ACTION_H_
