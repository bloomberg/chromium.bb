// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioParamMap_h
#define AudioParamMap_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Maplike.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "modules/webaudio/AudioParam.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class AudioParam;

class AudioParamMap final : public GarbageCollectedFinalized<AudioParamMap>,
                            public ScriptWrappable,
                            public Maplike<String, AudioParam*> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AudioParamMap(
      const HeapHashMap<String, Member<AudioParam>>& parameter_map);

  // IDL attributes / methods
  size_t size() const { return parameter_map_.size(); }

  AudioParam* At(String name) { return parameter_map_.at(name); }
  bool Contains(String name) { return parameter_map_.Contains(name); }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->Trace(parameter_map_); }

 private:
  PairIterable<String, AudioParam*>::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) override;
  bool GetMapEntry(ScriptState*,
                   const String& key,
                   AudioParam*&,
                   ExceptionState&) override;

  const HeapHashMap<String, Member<AudioParam>> parameter_map_;
};

}  // namespace blink

#endif
