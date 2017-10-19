/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorBaseAgent_h
#define InspectorBaseAgent_h

#include "core/CoreExport.h"
#include "core/CoreProbeSink.h"
#include "core/inspector/protocol/Protocol.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT InspectorAgent
    : public GarbageCollectedFinalized<InspectorAgent> {
 public:
  InspectorAgent() {}
  virtual ~InspectorAgent() {}
  virtual void Trace(blink::Visitor* visitor) {}

  virtual void Restore() {}
  virtual void DidCommitLoadForLocalFrame(LocalFrame*) {}
  virtual void FlushPendingProtocolNotifications() {}

  virtual void Init(CoreProbeSink*,
                    protocol::UberDispatcher*,
                    protocol::DictionaryValue*) = 0;
  virtual void Dispose() = 0;
};

template <typename DomainMetainfo>
class InspectorBaseAgent : public InspectorAgent,
                           public DomainMetainfo::BackendClass {
 public:
  ~InspectorBaseAgent() override {}

  void Init(CoreProbeSink* instrumenting_agents,
            protocol::UberDispatcher* dispatcher,
            protocol::DictionaryValue* state) override {
    instrumenting_agents_ = instrumenting_agents;
    frontend_.reset(
        new typename DomainMetainfo::FrontendClass(dispatcher->channel()));
    DomainMetainfo::DispatcherClass::wire(dispatcher, this);

    state_ = state->getObject(DomainMetainfo::domainName);
    if (!state_) {
      std::unique_ptr<protocol::DictionaryValue> new_state =
          protocol::DictionaryValue::create();
      state_ = new_state.get();
      state->setObject(DomainMetainfo::domainName, std::move(new_state));
    }
  }

  protocol::Response disable() override { return protocol::Response::OK(); }

  void Dispose() override {
    disable();
    frontend_.reset();
    state_ = nullptr;
    instrumenting_agents_ = nullptr;
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(instrumenting_agents_);
    InspectorAgent::Trace(visitor);
  }

 protected:
  InspectorBaseAgent() {}

  typename DomainMetainfo::FrontendClass* GetFrontend() const {
    return frontend_.get();
  }
  Member<CoreProbeSink> instrumenting_agents_;
  protocol::DictionaryValue* state_;

 private:
  std::unique_ptr<typename DomainMetainfo::FrontendClass> frontend_;
};

}  // namespace blink

#endif  // !defined(InspectorBaseAgent_h)
