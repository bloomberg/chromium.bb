// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_STREAM_WRAPPER_H_

#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace net {

class IPEndPoint;

}  // namespace net

namespace blink {

class ExceptionState;

class ReadableStream;
class WritableStream;

class ScriptValue;

}  // namespace blink

namespace blink {

class MODULES_EXPORT StreamWrapper : public GarbageCollectedMixin {
 public:
  enum class State { kOpen, kAborted, kClosed };

  explicit StreamWrapper(ScriptState*);
  virtual ~StreamWrapper() = 0;

  State GetState() const { return state_; }
  ScriptState* GetScriptState() const { return script_state_; }

  virtual bool Locked() const = 0;

  virtual void CloseStream(bool error) = 0;
  virtual void CloseSocket(bool error) { NOTIMPLEMENTED(); }

  void Trace(Visitor* visitor) const override;

 protected:
  void SetState(State state) { state_ = state; }

  static ScriptValue CreateException(ScriptState*,
                                     DOMExceptionCode,
                                     const String& message);

 private:
  const Member<ScriptState> script_state_;
  State state_ = State::kOpen;
};

class ReadableStreamWrapper : public StreamWrapper {
 public:
  explicit ReadableStreamWrapper(ScriptState*);

  ReadableStream* Readable() const { return readable_; }
  bool Locked() const override;

  // Implements UnderlyingSource::pull(...)
  virtual void Pull() = 0;

  // Enqueues the data in the stream controller queue.
  virtual bool Push(base::span<const uint8_t> data,
                    const absl::optional<net::IPEndPoint>& src_addr) = 0;

  void Trace(Visitor*) const override;

 protected:
  class UnderlyingSource;
  void InitSourceAndReadable(UnderlyingSource*, size_t high_water_mark);

  ReadableStreamDefaultControllerWithScriptScope* Controller() const;

 private:
  Member<UnderlyingSource> source_;
  Member<ReadableStream> readable_;
};

class WritableStreamWrapper : public StreamWrapper {
 public:
  explicit WritableStreamWrapper(ScriptState*);

  WritableStream* Writable() const { return writable_; }
  bool Locked() const override;

  // Implements UnderlyingSink::write(...)
  virtual ScriptPromise Write(ScriptValue, ExceptionState&) = 0;

  // Checks whether there's a write in progress.
  virtual bool HasPendingWrite() const { return false; }

  void Trace(Visitor*) const override;

 protected:
  class UnderlyingSink;
  void InitSinkAndWritable(UnderlyingSink*, size_t high_water_mark);

  WritableStreamDefaultController* Controller() const;

 private:
  Member<UnderlyingSink> sink_;
  Member<WritableStream> writable_;
};

class ReadableStreamWrapper::UnderlyingSource : public UnderlyingSourceBase {
 public:
  UnderlyingSource(ScriptState*, ReadableStreamWrapper*);

  ScriptPromise Start(ScriptState*) override;
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override = 0;

  void Trace(Visitor*) const override;

 protected:
  ReadableStreamWrapper* GetReadableStreamWrapper() const {
    return readable_stream_wrapper_;
  }

 private:
  friend class ReadableStreamWrapper;
  const Member<ReadableStreamWrapper> readable_stream_wrapper_;
};

class WritableStreamWrapper::UnderlyingSink : public UnderlyingSinkBase {
 public:
  explicit UnderlyingSink(WritableStreamWrapper*);

  ScriptPromise start(ScriptState*,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise write(ScriptState*,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise close(ScriptState*, ExceptionState&) override = 0;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;

  void Trace(Visitor*) const override;

 protected:
  WritableStreamWrapper* GetWritableStreamWrapper() const {
    return writable_stream_wrapper_;
  }

 private:
  friend class WritableStreamWrapper;
  const Member<WritableStreamWrapper> writable_stream_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_STREAM_WRAPPER_H_
