// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoHandle_h
#define MojoHandle_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "mojo/public/cpp/system/core.h"

namespace blink {

class ArrayBufferOrArrayBufferView;
class MojoHandleSignals;
class MojoReadMessageFlags;
class MojoReadMessageResult;
class MojoWatchCallback;
class MojoWatcher;
class ScriptState;

class MojoHandle final : public GarbageCollectedFinalized<MojoHandle>,
                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CORE_EXPORT static MojoHandle* create(mojo::ScopedHandle);

  void close();
  MojoWatcher* watch(ScriptState*,
                     const MojoHandleSignals&,
                     MojoWatchCallback*);
  MojoResult writeMessage(ArrayBufferOrArrayBufferView&,
                          const HeapVector<Member<MojoHandle>>&);
  void readMessage(const MojoReadMessageFlags&, MojoReadMessageResult&);

  DEFINE_INLINE_TRACE() {}

 private:
  explicit MojoHandle(mojo::ScopedHandle);

  mojo::ScopedHandle m_handle;
};

}  // namespace blink

#endif  // MojoHandle_h
