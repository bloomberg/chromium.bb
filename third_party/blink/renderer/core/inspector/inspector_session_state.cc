// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"

#include "third_party/blink/renderer/core/inspector/protocol/Protocol.h"

namespace blink {

//
// InspectorSessionState
//
InspectorSessionState::InspectorSessionState(
    mojom::blink::DevToolsSessionStatePtr reattach)
    : reattach_state_(std::move(reattach)),
      updates_(mojom::blink::DevToolsSessionState::New()) {}

const mojom::blink::DevToolsSessionState* InspectorSessionState::ReattachState()
    const {
  return reattach_state_.get();
}

void InspectorSessionState::EnqueueUpdate(const WTF::String& key,
                                          const WTF::String& value) {
  updates_->entries.Set(key, value);
}

mojom::blink::DevToolsSessionStatePtr InspectorSessionState::TakeUpdates() {
  auto updates = std::move(updates_);
  updates_ = mojom::blink::DevToolsSessionState::New();
  return updates;
}

//
// Encoding / Decoding routines.
//
/*static*/
void InspectorAgentState::EncodeToJSON(bool v, WTF::String* out) {
  std::unique_ptr<protocol::FundamentalValue> value =
      blink::protocol::FundamentalValue::create(v);
  *out = value->serialize();
}

/*static*/
bool InspectorAgentState::DecodeFromJSON(const WTF::String& in, bool* v) {
  std::unique_ptr<protocol::Value> parsed = protocol::StringUtil::parseJSON(in);
  return parsed->asBoolean(v);
}

/*static*/
void InspectorAgentState::EncodeToJSON(int32_t v, WTF::String* out) {
  std::unique_ptr<protocol::FundamentalValue> value =
      blink::protocol::FundamentalValue::create(v);
  *out = value->serialize();
}

/*static*/
bool InspectorAgentState::DecodeFromJSON(const WTF::String& in, int32_t* v) {
  std::unique_ptr<protocol::Value> parsed = protocol::StringUtil::parseJSON(in);
  return parsed->asInteger(v);
}

/*static*/
void InspectorAgentState::EncodeToJSON(double v, WTF::String* out) {
  std::unique_ptr<protocol::FundamentalValue> value =
      blink::protocol::FundamentalValue::create(v);
  *out = value->serialize();
}

/*static*/
bool InspectorAgentState::DecodeFromJSON(const WTF::String& in, double* v) {
  std::unique_ptr<protocol::Value> parsed = protocol::StringUtil::parseJSON(in);
  return parsed->asDouble(v);
}

/*static*/
void InspectorAgentState::EncodeToJSON(const WTF::String& v, WTF::String* out) {
  std::unique_ptr<protocol::StringValue> value =
      protocol::StringValue::create(v);
  *out = value->serialize();
}

/*static*/
bool InspectorAgentState::DecodeFromJSON(const WTF::String& in,
                                         WTF::String* v) {
  std::unique_ptr<protocol::Value> parsed = protocol::StringUtil::parseJSON(in);
  return parsed->asString(v);
}

//
// InspectorAgentState
//
InspectorAgentState::InspectorAgentState(const WTF::String& domain_name)
    : domain_name_(domain_name) {}

WTF::String InspectorAgentState::RegisterField(Field* field) {
  WTF::String prefix_key =
      domain_name_ + "." + WTF::String::Number(fields_.size()) + "/";
  fields_.push_back(field);
  return prefix_key;
}

void InspectorAgentState::InitFrom(InspectorSessionState* session_state) {
  for (Field* f : fields_)
    f->InitFrom(session_state);
}

void InspectorAgentState::Clear() {
  for (Field* f : fields_)
    f->Clear();
}

}  // namespace blink
