// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_audio_capturer.h"

#include "remoting/host/desktop_session_proxy.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

IpcAudioCapturer::IpcAudioCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {
  // The audio capturer is created on the network thread while used on the audio
  // capture thread. Detach |thread_checker_| from the current thread so that it
  // will re-attach to the proper thread when Start() is called for the first
  // time.
  thread_checker_.DetachFromThread();
}

IpcAudioCapturer::~IpcAudioCapturer() {
  DCHECK(callback_.is_null());
}

bool IpcAudioCapturer::Start(const PacketCapturedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  desktop_session_proxy_->StartAudioCapturer(this);

  return true;
}

void IpcAudioCapturer::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback_.is_null());

  desktop_session_proxy_->StopAudioCapturer();
  callback_.Reset();
}

bool IpcAudioCapturer::IsStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return !callback_.is_null();
}

// Called when an audio packet has been received.
void IpcAudioCapturer::OnAudioPacket(scoped_ptr<AudioPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback_.is_null());

  callback_.Run(packet.Pass());
}

}  // namespace remoting
