// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemotePlaybackConnectionCallbacks_h
#define RemotePlaybackConnectionCallbacks_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"

namespace blink {

class RemotePlayback;
struct WebPresentationError;
struct WebPresentationInfo;

// RemotePlaybackConnectionCallbacks extends WebCallbacks to handle the result
// of RemotePlayback.prompt().
class RemotePlaybackConnectionCallbacks final
    : public WebPresentationConnectionCallbacks {
 public:
  explicit RemotePlaybackConnectionCallbacks(RemotePlayback*);
  ~RemotePlaybackConnectionCallbacks() override = default;

  // WebCallbacks implementation.
  void OnSuccess(const WebPresentationInfo&) override;
  void OnError(const WebPresentationError&) override;

  // WebPresentationConnectionCallbacks implementation.
  WebPresentationConnection* GetConnection() override;

 private:
  WeakPersistent<RemotePlayback> remote_;

  WTF_MAKE_NONCOPYABLE(RemotePlaybackConnectionCallbacks);
};

}  // namespace blink

#endif  // RemotePlaybackConnectionCallbacks_h
