/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef FontFaceSetDocument_h
#define FontFaceSetDocument_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/FontFace.h"
#include "core/css/FontFaceSet.h"
#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/dom/PausableObject.h"
#include "core/dom/events/EventListener.h"
#include "core/dom/events/EventTarget.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Font;

class CORE_EXPORT FontFaceSetDocument final : public FontFaceSet,
                                              public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(FontFaceSetDocument);
  WTF_MAKE_NONCOPYABLE(FontFaceSetDocument);

 public:
  ~FontFaceSetDocument() override;

  ScriptPromise ready(ScriptState*) override;

  AtomicString status() const override;

  Document* GetDocument() const;

  void DidLayout();
  void BeginFontLoading(FontFace*);

  // FontFace::LoadFontCallback
  void NotifyLoaded(FontFace*) override;
  void NotifyError(FontFace*) override;

  size_t ApproximateBlankCharacterCount() const;

  static FontFaceSetDocument* From(Document&);
  static void DidLayout(Document&);
  static size_t ApproximateBlankCharacterCount(Document&);

  static const char* SupplementName() { return "FontFaceSetDocument"; }

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 protected:
  bool InActiveContext() const override;
  FontSelector* GetFontSelector() const override {
    return GetDocument()->GetStyleEngine().GetFontSelector();
  }

  bool ResolveFontStyle(const String&, Font&) override;

 private:
  static FontFaceSetDocument* Create(Document& document) {
    return new FontFaceSetDocument(document);
  }

  explicit FontFaceSetDocument(Document&);

  void FireDoneEventIfPossible() override;
  const HeapListHashSet<Member<FontFace>>& CSSConnectedFontFaceList()
      const override;

  class FontLoadHistogram {
    DISALLOW_NEW();

   public:
    enum Status { kNoWebFonts, kHadBlankText, kDidNotHaveBlankText, kReported };
    FontLoadHistogram() : status_(kNoWebFonts), count_(0), recorded_(false) {}
    void IncrementCount() { count_++; }
    void UpdateStatus(FontFace*);
    void Record();

   private:
    Status status_;
    int count_;
    bool recorded_;
  };
  FontLoadHistogram histogram_;
};

}  // namespace blink

#endif  // FontFaceSetDocument_h
