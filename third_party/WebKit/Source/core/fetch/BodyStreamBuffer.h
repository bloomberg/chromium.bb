// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BodyStreamBuffer_h
#define BodyStreamBuffer_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/DOMException.h"
#include "core/fetch/BytesConsumer.h"
#include "core/fetch/FetchDataLoader.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"

namespace blink {

class EncodedFormData;
class ScriptState;

class CORE_EXPORT BodyStreamBuffer final : public UnderlyingSourceBase,
                                           public BytesConsumer::Client {
  WTF_MAKE_NONCOPYABLE(BodyStreamBuffer);
  USING_GARBAGE_COLLECTED_MIXIN(BodyStreamBuffer);

 public:
  // |consumer| must not have a client.
  // This function must be called with entering an appropriate V8 context.
  BodyStreamBuffer(ScriptState*, BytesConsumer* /* consumer */);
  // |ReadableStreamOperations::isReadableStream(stream)| must hold.
  // This function must be called with entering an appropriate V8 context.
  BodyStreamBuffer(ScriptState*, ScriptValue stream);

  ScriptValue Stream();

  // Callable only when neither locked nor disturbed.
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy);
  scoped_refptr<EncodedFormData> DrainAsFormData();
  void StartLoading(FetchDataLoader*, FetchDataLoader::Client* /* client */);
  void Tee(BodyStreamBuffer**, BodyStreamBuffer**);

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;
  bool HasPendingActivity() const override;
  void ContextDestroyed(ExecutionContext*) override;

  // BytesConsumer::Client
  void OnStateChange() override;
  String DebugName() const override { return "BodyStreamBuffer"; }

  bool IsStreamReadable();
  bool IsStreamClosed();
  bool IsStreamErrored();
  bool IsStreamLocked();
  bool IsStreamDisturbed();
  void CloseAndLockAndDisturb();
  ScriptState* GetScriptState() { return script_state_.get(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    visitor->Trace(loader_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  class LoaderClient;

  BytesConsumer* ReleaseHandle();
  void Close();
  void GetError();
  void CancelConsumer();
  void ProcessData();
  void EndLoading();
  void StopLoading();

  scoped_refptr<ScriptState> script_state_;
  Member<BytesConsumer> consumer_;
  // We need this member to keep it alive while loading.
  Member<FetchDataLoader> loader_;
  bool stream_needs_more_ = false;
  bool made_from_readable_stream_;
  bool in_process_data_ = false;
};

}  // namespace blink

#endif  // BodyStreamBuffer_h
