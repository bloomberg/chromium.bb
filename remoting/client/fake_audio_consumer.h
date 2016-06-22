// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_FAKE_AUDIO_CONSUMER_H_
#define REMOTING_CLIENT_FAKE_AUDIO_CONSUMER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/audio_consumer.h"

namespace remoting {

class FakeAudioConsumer : public AudioConsumer {
 public:
  FakeAudioConsumer();
  ~FakeAudioConsumer() override;

  base::WeakPtr<FakeAudioConsumer> GetWeakPtr();

  // AudioConsumer implementation.
  void AddAudioPacket(std::unique_ptr<AudioPacket> packet) override;

 private:
  base::WeakPtrFactory<FakeAudioConsumer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioConsumer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FAKE_AUDIO_CONSUMER_H_
