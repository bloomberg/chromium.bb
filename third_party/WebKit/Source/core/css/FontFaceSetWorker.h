// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFaceSetWorker_h
#define FontFaceSetWorker_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/css/FontFace.h"
#include "core/css/FontFaceSet.h"
#include "core/css/OffscreenFontSelector.h"
#include "core/dom/PausableObject.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Font;

class CORE_EXPORT FontFaceSetWorker final
    : public FontFaceSet,
      public Supplement<WorkerGlobalScope> {
  USING_GARBAGE_COLLECTED_MIXIN(FontFaceSetWorker);
  WTF_MAKE_NONCOPYABLE(FontFaceSetWorker);

 public:
  ~FontFaceSetWorker() override;

  ScriptPromise ready(ScriptState*) override;

  AtomicString status() const override;

  WorkerGlobalScope* GetWorker() const;

  // FontFace::LoadFontCallback
  void NotifyLoaded(FontFace*) override;
  void NotifyError(FontFace*) override;

  void BeginFontLoading(FontFace*);

  static FontFaceSetWorker* From(WorkerGlobalScope&);

  static const char* SupplementName() { return "FontFaceSetWorker"; }

  void Trace(Visitor*) override;

 protected:
  bool InActiveContext() const override { return true; }
  FontSelector* GetFontSelector() const override {
    return GetWorker()->GetFontSelector();
  }
  // For workers, this is always an empty list.
  const HeapListHashSet<Member<FontFace>>& CSSConnectedFontFaceList()
      const override {
    DCHECK(
        GetFontSelector()->GetFontFaceCache()->CssConnectedFontFaces().size() ==
        0);
    return GetFontSelector()->GetFontFaceCache()->CssConnectedFontFaces();
  }

  bool ResolveFontStyle(const String&, Font&) override;

 private:
  static FontFaceSetWorker* Create(WorkerGlobalScope& worker) {
    return new FontFaceSetWorker(worker);
  }

  explicit FontFaceSetWorker(WorkerGlobalScope&);

  void FireDoneEventIfPossible() override;
};

}  // namespace blink

#endif  // FontFaceSetWorker_h
