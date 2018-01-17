// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/testing/InternalsWebAudio.h"

#include "modules/webaudio/AudioNode.h"
#include "platform/InstanceCounters.h"

namespace blink {

unsigned InternalsWebAudio::audioHandlerCount(Internals& internals) {
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(
      stderr, "InternalsWebAudio::audioHandlerCount = %u\n",
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter));
#endif
  return InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter);
}

}  // namespace blink
