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
  static AudioContext* create(Document&,
                              const AudioContextOptions&,
                              ExceptionState&);

  ~AudioContext() override;
  DECLARE_VIRTUAL_TRACE();

  ScriptPromise closeContext(ScriptState*);
  bool isContextClosed() const final;

  ScriptPromise suspendContext(ScriptState*) final;
  ScriptPromise resumeContext(ScriptState*) final;

  bool hasRealtimeConstraint() final { return true; }

  void getOutputTimestamp(ScriptState*, AudioTimestamp&);
  double baseLatency() const;

 protected:
  AudioContext(Document&, const WebAudioLatencyHint&);

  void didClose() final;

 private:
  void stopRendering();

  unsigned m_contextId;
  Member<ScriptPromiseResolver> m_closeResolver;
};

}  // namespace blink

#endif  // AudioContext_h
