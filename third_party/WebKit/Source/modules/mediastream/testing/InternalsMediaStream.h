// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsMediaStream_h
#define InternalsMediaStream_h

#include "platform/wtf/Allocator.h"

namespace blink {

class Internals;
class MediaDeviceInfo;
class MediaStreamTrack;
class MediaTrackConstraints;
class ScriptPromise;
class ScriptState;

class InternalsMediaStream {
  STATIC_ONLY(InternalsMediaStream);

 public:
  static ScriptPromise addFakeDevice(ScriptState*,
                                     Internals&,
                                     const MediaDeviceInfo*,
                                     const MediaTrackConstraints& capabilities,
                                     const MediaStreamTrack* data_source);
};

}  // namespace blink

#endif  // InternalsMediaStream_h
