/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NavigatorWebMIDI_h
#define NavigatorWebMIDI_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/frame/Navigator.h"
#include "modules/webmidi/MIDIOptions.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;

class NavigatorWebMIDI final : public GarbageCollected<NavigatorWebMIDI>,
                               public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorWebMIDI);

 public:
  static NavigatorWebMIDI& From(Navigator&);
  static ScriptPromise requestMIDIAccess(ScriptState*,
                                         Navigator&,
                                         const MIDIOptions&);
  ScriptPromise requestMIDIAccess(ScriptState*, const MIDIOptions&);

  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorWebMIDI(Navigator&);
  static const char* SupplementName();
};

}  // namespace blink

#endif  // NavigatorWebMIDI_h
