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

#ifndef InspectorDOMDebuggerAgent_h
#define InspectorDOMDebuggerAgent_h

#include "bindings/core/v8/V8EventListenerInfo.h"
#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/protocol/DOMDebugger.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class Element;
class InspectorDOMAgent;
class Node;

namespace probe {
class ExecuteScript;
class UserCallback;
}

namespace protocol {
class DictionaryValue;
}

class CORE_EXPORT InspectorDOMDebuggerAgent final
    : public InspectorBaseAgent<protocol::DOMDebugger::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);

 public:
  static void EventListenersInfoForTarget(v8::Isolate*,
                                          v8::Local<v8::Value>,
                                          V8EventListenerInfoList* listeners);

  InspectorDOMDebuggerAgent(v8::Isolate*,
                            InspectorDOMAgent*,
                            v8_inspector::V8InspectorSession*);
  ~InspectorDOMDebuggerAgent() override;
  void Trace(blink::Visitor*) override;

  // DOMDebugger API for frontend
  protocol::Response setDOMBreakpoint(int node_id, const String& type) override;
  protocol::Response removeDOMBreakpoint(int node_id,
                                         const String& type) override;
  protocol::Response setEventListenerBreakpoint(
      const String& event_name,
      protocol::Maybe<String> target_name) override;
  protocol::Response removeEventListenerBreakpoint(
      const String& event_name,
      protocol::Maybe<String> target_name) override;
  protocol::Response setInstrumentationBreakpoint(
      const String& event_name) override;
  protocol::Response removeInstrumentationBreakpoint(
      const String& event_name) override;
  protocol::Response setXHRBreakpoint(const String& url) override;
  protocol::Response removeXHRBreakpoint(const String& url) override;
  protocol::Response getEventListeners(
      const String& object_id,
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> pierce,
      std::unique_ptr<protocol::Array<protocol::DOMDebugger::EventListener>>*
          listeners) override;

  // InspectorInstrumentation API
  void WillInsertDOMNode(Node* parent);
  void DidInvalidateStyleAttr(Node*);
  void DidInsertDOMNode(Node*);
  void WillRemoveDOMNode(Node*);
  void DidRemoveDOMNode(Node*);
  void WillModifyDOMAttr(Element*, const AtomicString&, const AtomicString&);
  void WillSendXMLHttpOrFetchNetworkRequest(const String& url);
  void DidCreateCanvasContext();
  void DidFireWebGLError(const String& error_name);
  void DidFireWebGLWarning();
  void DidFireWebGLErrorOrWarning(const String& message);
  void ScriptExecutionBlockedByCSP(const String& directive_text);
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);
  void Will(const probe::UserCallback&);
  void Did(const probe::UserCallback&);
  void BreakableLocation(const char* name);

  protocol::Response disable() override;
  void Restore() override;
  void DidCommitLoadForLocalFrame(LocalFrame*) override;

 private:
  static void EventListenersInfoForTarget(v8::Isolate*,
                                          v8::Local<v8::Value>,
                                          int depth,
                                          bool pierce,
                                          V8EventListenerInfoList* listeners);
  void AllowNativeBreakpoint(const String& breakpoint_name,
                             const String* target_name,
                             bool sync);
  void CancelNativeBreakpoint();
  void PauseOnNativeEventIfNeeded(
      std::unique_ptr<protocol::DictionaryValue> event_data,
      bool synchronous);
  std::unique_ptr<protocol::DictionaryValue> PreparePauseOnNativeEventData(
      const String& event_name,
      const String* target_name);

  protocol::DictionaryValue* EventListenerBreakpoints();
  protocol::DictionaryValue* XhrBreakpoints();

  void BreakProgramOnDOMEvent(Node* target,
                              int breakpoint_type,
                              bool insertion);
  void UpdateSubtreeBreakpoints(Node*, uint32_t root_mask, bool set);
  bool HasBreakpoint(Node*, int type);
  protocol::Response SetBreakpoint(const String& event_name,
                                   const String& target_name);
  protocol::Response RemoveBreakpoint(const String& event_name,
                                      const String& target_name);

  void DidAddBreakpoint();
  void DidRemoveBreakpoint();
  void SetEnabled(bool);

  std::unique_ptr<protocol::DOMDebugger::EventListener>
  BuildObjectForEventListener(v8::Local<v8::Context>,
                              const V8EventListenerInfo&,
                              const v8_inspector::StringView& object_group_id);

  v8::Isolate* isolate_;
  Member<InspectorDOMAgent> dom_agent_;
  v8_inspector::V8InspectorSession* v8_session_;
  HeapHashMap<Member<Node>, uint32_t> dom_breakpoints_;
};

}  // namespace blink

#endif  // !defined(InspectorDOMDebuggerAgent_h)
