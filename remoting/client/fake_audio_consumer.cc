// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/fake_audio_consumer.h"

#include "remoting/proto/audio.pb.h"

namespace remoting {

FakeAudioConsumer::FakeAudioConsumer() : weak_factory_(this) {}

FakeAudioConsumer::~FakeAudioConsumer() {}

void FakeAudioConsumer::AddAudioPacket(std::unique_ptr<AudioPacket> packet) {}

base::WeakPtr<FakeAudioConsumer> FakeAudioConsumer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
