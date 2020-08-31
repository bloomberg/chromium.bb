// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_H_

#include <memory>
#include "base/optional.h"
#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace blink {

class EncodedFormData;
class ExceptionState;
class ReadableStream;
class ScriptState;

class CORE_EXPORT BodyStreamBuffer final : public UnderlyingSourceBase,
                                           public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BodyStreamBuffer);

 public:
  using PassKey = util::PassKey<BodyStreamBuffer>;

  // Create a BodyStreamBuffer for |consumer|.
  // |consumer| must not have a client.
  // This function must be called after entering an appropriate V8 context.
  // |signal| should be non-null when this BodyStreamBuffer is associated with a
  // Response that was created by fetch().
  static BodyStreamBuffer* Create(
      ScriptState*,
      BytesConsumer* consumer,
      AbortSignal* signal,
      scoped_refptr<BlobDataHandle> side_data_blob = nullptr);

  // Create() should be used instead of calling this constructor directly.
  BodyStreamBuffer(PassKey,
                   ScriptState*,
                   BytesConsumer* consumer,
                   AbortSignal* signal,
                   scoped_refptr<BlobDataHandle> side_data_blob);

  BodyStreamBuffer(ScriptState*,
                   ReadableStream* stream,
                   scoped_refptr<BlobDataHandle> side_data_blob = nullptr);

  ReadableStream* Stream() { return stream_; }

  // Callable only when neither locked nor disturbed.
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy,
      ExceptionState&);
  scoped_refptr<EncodedFormData> DrainAsFormData(ExceptionState&);
  void StartLoading(FetchDataLoader*,
                    FetchDataLoader::Client* /* client */,
                    ExceptionState&);
  void Tee(BodyStreamBuffer**, BodyStreamBuffer**, ExceptionState&);

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;
  bool HasPendingActivity() const override;
  void ContextDestroyed() override;

  // BytesConsumer::Client
  void OnStateChange() override;
  String DebugName() const override { return "BodyStreamBuffer"; }

  base::Optional<bool> IsStreamReadable(ExceptionState&);
  base::Optional<bool> IsStreamClosed(ExceptionState&);
  base::Optional<bool> IsStreamErrored(ExceptionState&);
  base::Optional<bool> IsStreamLocked(ExceptionState&);
  bool IsStreamLockedForDCheck(ExceptionState&);
  base::Optional<bool> IsStreamDisturbed(ExceptionState&);
  bool IsStreamDisturbedForDCheck(ExceptionState&);
  void CloseAndLockAndDisturb(ExceptionState&);
  ScriptState* GetScriptState() { return script_state_; }

  bool IsAborted();

  // Take the blob representing any side data associated with this body
  // stream.  This must be called before the body is drained or begins
  // loading.
  scoped_refptr<BlobDataHandle> TakeSideDataBlob();
  scoped_refptr<BlobDataHandle> GetSideDataBlobForTest() const {
    return side_data_blob_;
  }

  void Trace(Visitor*) override;

 private:
  class LoaderClient;

  // This method exists to avoid re-entrancy inside the BodyStreamBuffer
  // constructor. It is called by Create(). It should not be called after
  // using the ReadableStream* constructor.
  void Init();

  BytesConsumer* ReleaseHandle(ExceptionState&);
  void Abort();
  void Close();
  void GetError();
  void CancelConsumer();
  void ProcessData();
  void EndLoading();
  void StopLoading();

  // Implementation of IsStream*() methods. Delegates to |predicate|, one of the
  // methods defined in ReadableStream. Sets |stream_broken_| and throws if
  // |predicate| throws. Throws an exception if called when |stream_broken_|
  // is already true.
  base::Optional<bool> BooleanStreamOperation(
      base::Optional<bool> (ReadableStream::*predicate)(ScriptState*,
                                                        ExceptionState&) const,
      ExceptionState& exception_state);

  Member<ScriptState> script_state_;
  Member<ReadableStream> stream_;
  Member<BytesConsumer> consumer_;
  // We need this member to keep it alive while loading.
  Member<FetchDataLoader> loader_;
  // We need this to ensure that we detect that abort has been signalled
  // correctly.
  Member<AbortSignal> signal_;
  // Additional side data associated with this body stream.  It should only be
  // retained until the body is drained or starts loading.  Client code, such
  // as service workers, can call TakeSideDataBlob() prior to consumption.
  scoped_refptr<BlobDataHandle> side_data_blob_;
  bool stream_needs_more_ = false;
  bool made_from_readable_stream_;
  bool in_process_data_ = false;
  bool stream_broken_ = false;

  DISALLOW_COPY_AND_ASSIGN(BodyStreamBuffer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_H_
