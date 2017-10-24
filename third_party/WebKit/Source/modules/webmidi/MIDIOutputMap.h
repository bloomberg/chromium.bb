// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIOutputMap_h
#define MIDIOutputMap_h

#include "modules/webmidi/MIDIOutput.h"
#include "modules/webmidi/MIDIPortMap.h"

namespace blink {

class MIDIOutputMap : public MIDIPortMap<MIDIOutput> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MIDIOutputMap(HeapVector<Member<MIDIOutput>>&);
};

}  // namespace blink

#endif
