// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/ReadableStreamBytesConsumer.h"

#include <string.h>

#include <algorithm>

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "core/streams/ReadableStreamOperations.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

class ReadableStreamBytesConsumer::OnFulfilled final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      ReadableStreamBytesConsumer* consumer) {
    return (new OnFulfilled(script_state, consumer))->BindToV8Function();
  }

  ScriptValue Call(ScriptValue v) override {
    bool done;
    v8::Local<v8::Value> item = v.V8Value();
    DCHECK(item->IsObject());
    v8::Local<v8::Value> value =
        V8UnpackIteratorResult(v.GetScriptState(), item.As<v8::Object>(), &done)
            .ToLocalChecked();
    if (done) {
      consumer_->OnReadDone();
      return v;
    }
    if (!value->IsUint8Array()) {
      consumer_->OnRejected();
      return ScriptValue();
    }
    consumer_->OnRead(V8Uint8Array::ToImpl(value.As<v8::Object>()));
    return v;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  OnFulfilled(ScriptState* script_state, ReadableStreamBytesConsumer* consumer)
      : ScriptFunction(script_state), consumer_(consumer) {}

  Member<ReadableStreamBytesConsumer> consumer_;
};

class ReadableStreamBytesConsumer::OnRejected final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      ReadableStreamBytesConsumer* consumer) {
    return (new OnRejected(script_state, consumer))->BindToV8Function();
  }

  ScriptValue Call(ScriptValue v) override {
    consumer_->OnRejected();
    return v;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  OnRejected(ScriptState* script_state, ReadableStreamBytesConsumer* consumer)
      : ScriptFunction(script_state), consumer_(consumer) {}

  Member<ReadableStreamBytesConsumer> consumer_;
};

ReadableStreamBytesConsumer::ReadableStreamBytesConsumer(
    ScriptState* script_state,
    ScriptValue stream_reader)
    : reader_(script_state->GetIsolate(), stream_reader.V8Value()),
      script_state_(script_state) {
  reader_.SetPhantom();
}

ReadableStreamBytesConsumer::~ReadableStreamBytesConsumer() {}

BytesConsumer::Result ReadableStreamBytesConsumer::BeginRead(
    const char** buffer,
    size_t* available) {
  *buffer = nullptr;
  *available = 0;
  if (state_ == PublicState::kErrored)
    return Result::kError;
  if (state_ == PublicState::kClosed)
    return Result::kDone;

  if (pending_buffer_) {
    DCHECK_LE(pending_offset_, pending_buffer_->length());
    *buffer = reinterpret_cast<const char*>(pending_buffer_->Data()) +
              pending_offset_;
    *available = pending_buffer_->length() - pending_offset_;
    return Result::kOk;
  }
  if (!is_reading_) {
    is_reading_ = true;
    ScriptState::Scope scope(script_state_.get());
    ScriptValue reader(script_state_.get(),
                       reader_.NewLocal(script_state_->GetIsolate()));
    // The owner must retain the reader.
    DCHECK(!reader.IsEmpty());
    ReadableStreamOperations::DefaultReaderRead(script_state_.get(), reader)
        .Then(OnFulfilled::CreateFunction(script_state_.get(), this),
              OnRejected::CreateFunction(script_state_.get(), this));
  }
  return Result::kShouldWait;
}

BytesConsumer::Result ReadableStreamBytesConsumer::EndRead(size_t read_size) {
  DCHECK(pending_buffer_);
  DCHECK_LE(pending_offset_ + read_size, pending_buffer_->length());
  pending_offset_ += read_size;
  if (pending_offset_ >= pending_buffer_->length()) {
    pending_buffer_ = nullptr;
    pending_offset_ = 0;
  }
  return Result::kOk;
}

void ReadableStreamBytesConsumer::SetClient(Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  client_ = client;
}

void ReadableStreamBytesConsumer::ClearClient() {
  client_ = nullptr;
}

void ReadableStreamBytesConsumer::Cancel() {
  if (state_ == PublicState::kClosed || state_ == PublicState::kErrored)
    return;
  state_ = PublicState::kClosed;
  ClearClient();
  reader_.Clear();
}

BytesConsumer::PublicState ReadableStreamBytesConsumer::GetPublicState() const {
  return state_;
}

BytesConsumer::Error ReadableStreamBytesConsumer::GetError() const {
  return Error("Failed to read from a ReadableStream.");
}

void ReadableStreamBytesConsumer::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
  visitor->Trace(pending_buffer_);
  BytesConsumer::Trace(visitor);
}

void ReadableStreamBytesConsumer::Dispose() {
  reader_.Clear();
}

void ReadableStreamBytesConsumer::OnRead(DOMUint8Array* buffer) {
  DCHECK(is_reading_);
  DCHECK(buffer);
  DCHECK(!pending_buffer_);
  DCHECK(!pending_offset_);
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  pending_buffer_ = buffer;
  if (client_)
    client_->OnStateChange();
}

void ReadableStreamBytesConsumer::OnReadDone() {
  DCHECK(is_reading_);
  DCHECK(!pending_buffer_);
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  state_ = PublicState::kClosed;
  reader_.Clear();
  Client* client = client_;
  ClearClient();
  if (client)
    client->OnStateChange();
}

void ReadableStreamBytesConsumer::OnRejected() {
  DCHECK(is_reading_);
  DCHECK(!pending_buffer_);
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  state_ = PublicState::kErrored;
  reader_.Clear();
  Client* client = client_;
  ClearClient();
  if (client)
    client->OnStateChange();
}

}  // namespace blink
