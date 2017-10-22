// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorSession_h
#define InspectorSession_h

#include "core/CoreExport.h"
#include "core/inspector/protocol/Forward.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8-inspector-protocol.h"

namespace blink {

class InspectorAgent;
class CoreProbeSink;
class LocalFrame;

class CORE_EXPORT InspectorSession
    : public GarbageCollectedFinalized<InspectorSession>,
      public protocol::FrontendChannel,
      public v8_inspector::V8Inspector::Channel {
  WTF_MAKE_NONCOPYABLE(InspectorSession);

 public:
  class Client {
   public:
    virtual void SendProtocolMessage(int session_id,
                                     int call_id,
                                     const String& response,
                                     const String& state) = 0;
    virtual ~Client() {}
  };

  InspectorSession(Client*,
                   CoreProbeSink*,
                   int session_id,
                   v8_inspector::V8Inspector*,
                   int context_group_id,
                   const String* saved_state);
  ~InspectorSession() override;
  int SessionId() { return session_id_; }
  v8_inspector::V8InspectorSession* V8Session() { return v8_session_.get(); }

  void Append(InspectorAgent*);
  void Restore();
  void Dispose();
  void DidCommitLoadForLocalFrame(LocalFrame*);
  void DispatchProtocolMessage(const String& method, const String& message);
  void DispatchProtocolMessage(const String& message);
  void flushProtocolNotifications() override;

  void Trace(blink::Visitor*);

 private:
  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;

  // v8_inspector::V8Inspector::Channel implementation.
  void sendResponse(
      int call_id,
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  // TODO(kozyatinskiy): remove it.
  void SendProtocolResponse(int call_id,
                            const v8_inspector::StringView& message) {}
  void SendProtocolNotification(const v8_inspector::StringView& message) {}

  void SendProtocolResponse(int call_id, const String& message);

  Client* client_;
  std::unique_ptr<v8_inspector::V8InspectorSession> v8_session_;
  int session_id_;
  bool disposed_;
  Member<CoreProbeSink> instrumenting_agents_;
  std::unique_ptr<protocol::UberDispatcher> inspector_backend_dispatcher_;
  std::unique_ptr<protocol::DictionaryValue> state_;
  HeapVector<Member<InspectorAgent>> agents_;
  class Notification;
  Vector<std::unique_ptr<Notification>> notification_queue_;
  String last_sent_state_;
};

}  // namespace blink

#endif  // !defined(InspectorSession_h)
