// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipboardPromise_h
#define ClipboardPromise_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "public/platform/WebClipboard.h"

namespace blink {

class DataTransfer;
class ScriptPromiseResolver;

class ClipboardPromise final
    : public GarbageCollectedFinalized<ClipboardPromise>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ClipboardPromise);
  WTF_MAKE_NONCOPYABLE(ClipboardPromise);

 public:
  virtual ~ClipboardPromise(){};

  static ScriptPromise CreateForRead(ScriptState*);
  static ScriptPromise CreateForReadText(ScriptState*);
  static ScriptPromise CreateForWrite(ScriptState*, DataTransfer*);
  static ScriptPromise CreateForWriteText(ScriptState*, const String&);

  virtual void Trace(blink::Visitor*);

 private:
  ClipboardPromise(ScriptState*);

  scoped_refptr<WebTaskRunner> GetTaskRunner();

  void HandleRead();
  void HandleReadText();

  void HandleWrite(DataTransfer*);
  void HandleWriteText(const String&);

  Member<ScriptPromiseResolver> script_promise_resolver_;

  WebClipboard::Buffer buffer_;
};

}  // namespace blink

#endif  // ClipboardPromise_h
