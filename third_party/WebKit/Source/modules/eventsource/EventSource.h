/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef EventSource_h
#define EventSource_h

#include <memory>
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/ModulesExport.h"
#include "modules/eventsource/EventSourceParser.h"
#include "platform/Timer.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"

namespace blink {

class EventSourceInit;
class ExceptionState;
class ResourceResponse;

class MODULES_EXPORT EventSource final
    : public EventTargetWithInlineData,
      private ThreadableLoaderClient,
      public ActiveScriptWrappable<EventSource>,
      public ContextLifecycleObserver,
      public EventSourceParser::Client {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(EventSource);
  USING_PRE_FINALIZER(EventSource, Dispose);

 public:
  static EventSource* Create(ExecutionContext*,
                             const String& url,
                             const EventSourceInit&,
                             ExceptionState&);
  ~EventSource() override;

  static const unsigned long long kDefaultReconnectDelay;

  String url() const;
  bool withCredentials() const;

  enum State : short { kConnecting = 0, kOpen = 1, kClosed = 2 };

  State readyState() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

  void close();

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ContextLifecycleObserver
  //
  // Note: We don't need to inherit from SuspendableObject since
  // ScopedPageLoadDeferrer calls Page::setDefersLoading() and
  // it defers delivery of events from the loader, and therefore
  // the methods of this class for receiving asynchronous events
  // from the loader won't be invoked.
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  virtual void Trace(blink::Visitor*);

 private:
  EventSource(ExecutionContext*, const KURL&, const EventSourceInit&);

  void Dispose();

  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveData(const char*, unsigned) override;
  void DidFinishLoading(unsigned long, double) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void OnMessageEvent(const AtomicString& event,
                      const String& data,
                      const AtomicString& id) override;
  void OnReconnectionTimeSet(unsigned long long reconnection_time) override;

  void ScheduleInitialConnect();
  void Connect();
  void NetworkRequestEnded();
  void ScheduleReconnect();
  void ConnectTimerFired(TimerBase*);
  void AbortConnectionAttempt();

  // The original URL specified when constructing EventSource instance. Used
  // for the 'url' attribute getter.
  const KURL url_;
  // The URL used to connect to the server, which may be different from
  // |m_url| as it may be redirected.
  KURL current_url_;
  bool with_credentials_;
  State state_;

  Member<EventSourceParser> parser_;
  Member<ThreadableLoader> loader_;
  TaskRunnerTimer<EventSource> connect_timer_;

  unsigned long long reconnect_delay_;
  String event_stream_origin_;
};

}  // namespace blink

#endif  // EventSource_h
