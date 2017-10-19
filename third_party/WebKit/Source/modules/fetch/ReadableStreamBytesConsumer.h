// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamBytesConsumer_h
#define ReadableStreamBytesConsumer_h

#include <memory>
#include "bindings/core/v8/ScriptValue.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ScriptState;

// This class is a BytesConsumer pulling bytes from ReadableStream
// implemented with V8 Extras.
// The stream will be immediately locked by the consumer and will never be
// released.
//
// The ReadableStreamReader handle held in a ReadableStreamDataConsumerHandle
// is weak. A user must guarantee that the ReadableStreamReader object is kept
// alive appropriately.
class MODULES_EXPORT ReadableStreamBytesConsumer final : public BytesConsumer {
  WTF_MAKE_NONCOPYABLE(ReadableStreamBytesConsumer);
  USING_PRE_FINALIZER(ReadableStreamBytesConsumer, Dispose);

 public:
  ReadableStreamBytesConsumer(ScriptState*, ScriptValue stream_reader);
  ~ReadableStreamBytesConsumer() override;

  Result BeginRead(const char** buffer, size_t* available) override;
  Result EndRead(size_t read_size) override;
  void SetClient(BytesConsumer::Client*) override;
  void ClearClient() override;

  void Cancel() override;
  PublicState GetPublicState() const override;
  Error GetError() const override;
  String DebugName() const override { return "ReadableStreamBytesConsumer"; }

  void Trace(blink::Visitor*);

 private:
  class OnFulfilled;
  class OnRejected;

  void Dispose();
  void OnRead(DOMUint8Array*);
  void OnReadDone();
  void OnRejected();
  void Notify();

  // |m_reader| is a weak persistent. It should be kept alive by someone
  // outside of ReadableStreamBytesConsumer.
  // Holding a ScopedPersistent here is safe in terms of cross-world wrapper
  // leakage because we read only Uint8Array chunks from the reader.
  ScopedPersistent<v8::Value> reader_;
  RefPtr<ScriptState> script_state_;
  Member<BytesConsumer::Client> client_;
  Member<DOMUint8Array> pending_buffer_;
  size_t pending_offset_ = 0;
  PublicState state_ = PublicState::kReadableOrWaiting;
  bool is_reading_ = false;
};

}  // namespace blink

#endif  // ReadableStreamBytesConsumer_h
