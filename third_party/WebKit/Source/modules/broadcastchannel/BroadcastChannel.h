// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BroadcastChannel_h
#define BroadcastChannel_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/modules/broadcastchannel/broadcast_channel.mojom-blink.h"

namespace blink {

class ScriptValue;

class BroadcastChannel final : public EventTargetWithInlineData,
                               public ActiveScriptWrappable<BroadcastChannel>,
                               public ContextLifecycleObserver,
                               public mojom::blink::BroadcastChannelClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BroadcastChannel);
  USING_PRE_FINALIZER(BroadcastChannel, Dispose);
  WTF_MAKE_NONCOPYABLE(BroadcastChannel);

 public:
  static BroadcastChannel* Create(ExecutionContext*,
                                  const String& name,
                                  ExceptionState&);
  ~BroadcastChannel() override;
  void Dispose();

  // IDL
  String name() const { return name_; }
  void postMessage(const ScriptValue&, ExceptionState&);
  void close();
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(messageerror);

  // EventTarget:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }

  // ScriptWrappable:
  bool HasPendingActivity() const override;

  // ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  BroadcastChannel(ExecutionContext*, const String& name);

  // mojom::blink::BroadcastChannelClient:
  void OnMessage(BlinkCloneableMessage) override;

  // Called when the mojo binding disconnects.
  void OnError();

  scoped_refptr<SecurityOrigin> origin_;
  String name_;

  mojo::AssociatedBinding<mojom::blink::BroadcastChannelClient> binding_;
  mojom::blink::BroadcastChannelClientAssociatedPtr remote_client_;
};

}  // namespace blink

#endif  // BroadcastChannel_h
