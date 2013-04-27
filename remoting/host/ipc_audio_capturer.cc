// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_audio_capturer.h"

#include "remoting/host/desktop_session_proxy.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

IpcAudioCapturer::IpcAudioCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy),
      weak_factory_(this) {
}

IpcAudioCapturer::~IpcAudioCapturer() {
}

bool IpcAudioCapturer::Start(const PacketCapturedCallback& callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  desktop_session_proxy_->SetAudioCapturer(weak_factory_.GetWeakPtr());
  return true;
}

void IpcAudioCapturer::Stop() {
  callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

bool IpcAudioCapturer::IsStarted() {
  return !callback_.is_null();
}

void IpcAudioCapturer::OnAudioPacket(scoped_ptr<AudioPacket> packet) {
  callback_.Run(packet.Pass());
}

}  // namespace remoting
