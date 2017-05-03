// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIInputMap_h
#define MIDIInputMap_h

#include "modules/webmidi/MIDIInput.h"
#include "modules/webmidi/MIDIPortMap.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class MIDIInputMap : public MIDIPortMap<MIDIInput>, public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MIDIInputMap(const HeapVector<Member<MIDIInput>>&);
};

}  // namespace blink

#endif
