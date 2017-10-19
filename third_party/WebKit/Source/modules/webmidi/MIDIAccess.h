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

#ifndef MIDIAccess_h
#define MIDIAccess_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "media/midi/midi_service.mojom-blink.h"
#include "modules/EventTargetModules.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ExecutionContext;
class MIDIInput;
class MIDIInputMap;
class MIDIOutput;
class MIDIOutputMap;

class MIDIAccess final : public EventTargetWithInlineData,
                         public ActiveScriptWrappable<MIDIAccess>,
                         public ContextLifecycleObserver,
                         public MIDIAccessorClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MIDIAccess);
  USING_PRE_FINALIZER(MIDIAccess, Dispose);

 public:
  static MIDIAccess* Create(
      std::unique_ptr<MIDIAccessor> accessor,
      bool sysex_enabled,
      const Vector<MIDIAccessInitializer::PortDescriptor>& ports,
      ExecutionContext* execution_context) {
    return new MIDIAccess(std::move(accessor), sysex_enabled, ports,
                          execution_context);
  }
  ~MIDIAccess() override;

  MIDIInputMap* inputs() const;
  MIDIOutputMap* outputs() const;

  EventListener* onstatechange();
  void setOnstatechange(EventListener*);

  bool sysexEnabled() const { return sysex_enabled_; }

  // EventTarget
  const AtomicString& InterfaceName() const override {
    return EventTargetNames::MIDIAccess;
  }
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // MIDIAccessorClient
  void DidAddInputPort(const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       midi::mojom::PortState) override;
  void DidAddOutputPort(const String& id,
                        const String& manufacturer,
                        const String& name,
                        const String& version,
                        midi::mojom::PortState) override;
  void DidSetInputPortState(unsigned port_index,
                            midi::mojom::PortState) override;
  void DidSetOutputPortState(unsigned port_index,
                             midi::mojom::PortState) override;
  void DidStartSession(midi::mojom::Result) override {
    // This method is for MIDIAccess initialization: MIDIAccessInitializer
    // has the implementation.
    NOTREACHED();
  }
  void DidReceiveMIDIData(unsigned port_index,
                          const unsigned char* data,
                          size_t length,
                          double time_stamp) override;

  // |timeStampInMilliseconds| is in the same time coordinate system as
  // performance.now().
  void SendMIDIData(unsigned port_index,
                    const unsigned char* data,
                    size_t length,
                    double time_stamp_in_milliseconds);

  // Eager finalization needed to promptly release m_accessor. Otherwise
  // its client back reference could end up being unsafely used during
  // the lazy sweeping phase.
  virtual void Trace(blink::Visitor*);

 private:
  MIDIAccess(std::unique_ptr<MIDIAccessor>,
             bool sysex_enabled,
             const Vector<MIDIAccessInitializer::PortDescriptor>&,
             ExecutionContext*);
  void Dispose();

  std::unique_ptr<MIDIAccessor> accessor_;
  bool sysex_enabled_;
  bool has_pending_activity_;
  HeapVector<Member<MIDIInput>> inputs_;
  HeapVector<Member<MIDIOutput>> outputs_;
};

}  // namespace blink

#endif  // MIDIAccess_h
