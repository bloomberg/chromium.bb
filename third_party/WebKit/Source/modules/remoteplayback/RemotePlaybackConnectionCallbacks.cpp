// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlaybackConnectionCallbacks.h"

#include "modules/remoteplayback/RemotePlayback.h"

namespace blink {

RemotePlaybackConnectionCallbacks::RemotePlaybackConnectionCallbacks(
    RemotePlayback* remote)
    : remote_(remote) {
  DCHECK(remote_);
}

void RemotePlaybackConnectionCallbacks::OnSuccess(
    const WebPresentationInfo& presentation_info) {
  if (remote_)
    remote_->OnConnectionSuccess(presentation_info);
}

void RemotePlaybackConnectionCallbacks::OnError(
    const WebPresentationError& error) {
  if (remote_)
    remote_->OnConnectionError(error);
}

WebPresentationConnection* RemotePlaybackConnectionCallbacks::GetConnection() {
  return remote_ ? remote_.Get() : nullptr;
}

}  // namespace blink
