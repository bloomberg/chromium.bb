// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoHandle_h
#define MojoHandle_h

#include "core/CoreExport.h"
#include "mojo/public/cpp/system/core.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class ArrayBufferOrArrayBufferView;
class MojoCreateSharedBufferResult;
class MojoDiscardDataOptions;
class MojoDuplicateBufferHandleOptions;
class MojoHandleSignals;
class MojoMapBufferResult;
class MojoReadDataOptions;
class MojoReadDataResult;
class MojoReadMessageFlags;
class MojoReadMessageResult;
class MojoWatcher;
class MojoWriteDataOptions;
class MojoWriteDataResult;
class ScriptState;
class V8MojoWatchCallback;

class MojoHandle final : public GarbageCollectedFinalized<MojoHandle>,
                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CORE_EXPORT static MojoHandle* Create(mojo::ScopedHandle);

  mojo::ScopedHandle TakeHandle();

  void close();
  MojoWatcher* watch(ScriptState*,
                     const MojoHandleSignals&,
                     V8MojoWatchCallback*);

  // MessagePipe handle.
  MojoResult writeMessage(ArrayBufferOrArrayBufferView&,
                          const HeapVector<Member<MojoHandle>>&);
  void readMessage(const MojoReadMessageFlags&, MojoReadMessageResult&);

  // DataPipe handle.
  void writeData(const ArrayBufferOrArrayBufferView&,
                 const MojoWriteDataOptions&,
                 MojoWriteDataResult&);
  void queryData(MojoReadDataResult&);
  void discardData(unsigned num_bytes,
                   const MojoDiscardDataOptions&,
                   MojoReadDataResult&);
  void readData(ArrayBufferOrArrayBufferView&,
                const MojoReadDataOptions&,
                MojoReadDataResult&);

  // SharedBuffer handle.
  void mapBuffer(unsigned offset, unsigned num_bytes, MojoMapBufferResult&);
  void duplicateBufferHandle(const MojoDuplicateBufferHandleOptions&,
                             MojoCreateSharedBufferResult&);

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit MojoHandle(mojo::ScopedHandle);

  mojo::ScopedHandle handle_;
};

}  // namespace blink

#endif  // MojoHandle_h
