// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_
#define WEBKIT_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_

namespace webkit_media {

class WebMediaPlayerImpl;

// An interface to allow a WebMediaPlayerImpl to communicate changes of state
// to objects that need to know.
class WebMediaPlayerDelegate {
 public:
  WebMediaPlayerDelegate() {}
  virtual ~WebMediaPlayerDelegate() {}

  // The specified player started playing media.
  virtual void DidPlay(WebMediaPlayerImpl* player) {}

  // The specified player stopped playing media.
  virtual void DidPause(WebMediaPlayerImpl* player) {}

  // The specified player was destroyed. Do not call any methods on it.
  virtual void PlayerGone(WebMediaPlayerImpl* player) {}
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_
