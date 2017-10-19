// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioContext_h
#define AudioContext_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/webaudio/AudioContextOptions.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/heap/Handle.h"

namespace blink {

class AudioContextOptions;
class AudioTimestamp;
class Document;
class ExceptionState;
class ScriptState;
class WebAudioLatencyHint;

// This is an BaseAudioContext which actually plays sound, unlike an
// OfflineAudioContext which renders sound into a buffer.
class MODULES_EXPORT AudioContext : public BaseAudioContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioContext* Create(Document&,
                              const AudioContextOptions&,
                              ExceptionState&);

  ~AudioContext() override;
  virtual void Trace(blink::Visitor*);

  ScriptPromise closeContext(ScriptState*);
  bool IsContextClosed() const final;

  ScriptPromise suspendContext(ScriptState*) final;
  ScriptPromise resumeContext(ScriptState*) final;

  bool HasRealtimeConstraint() final { return true; }

  void getOutputTimestamp(ScriptState*, AudioTimestamp&);
  double baseLatency() const;

 protected:
  AudioContext(Document&, const WebAudioLatencyHint&);

  void DidClose() final;

 private:
  void StopRendering();

  unsigned context_id_;
  Member<ScriptPromiseResolver> close_resolver_;
};

}  // namespace blink

#endif  // AudioContext_h
