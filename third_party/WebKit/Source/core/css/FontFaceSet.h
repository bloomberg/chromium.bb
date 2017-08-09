// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFaceSet_h
#define FontFaceSet_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/css/FontFace.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"

// Mac OS X 10.6 SDK defines check() macro that interferes with our check()
// method
#ifdef check
#undef check
#endif

namespace blink {

using FontFaceSetIterable = SetlikeIterable<Member<FontFace>>;

class CORE_EXPORT FontFaceSet : public EventTargetWithInlineData,
                                public FontFaceSetIterable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(FontFaceSet);

 public:
  FontFaceSet() {}
  ~FontFaceSet() {}

  DEFINE_ATTRIBUTE_EVENT_LISTENER(loading);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingdone);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingerror);

  virtual bool check(const String& font,
                     const String& text,
                     ExceptionState&) = 0;
  virtual ScriptPromise load(ScriptState*,
                             const String& font,
                             const String& text) = 0;
  virtual ScriptPromise ready(ScriptState*) = 0;

  virtual ExecutionContext* GetExecutionContext() const = 0;
  const AtomicString& InterfaceName() const {
    return EventTargetNames::FontFaceSet;
  }

  virtual FontFaceSet* addForBinding(ScriptState*,
                                     FontFace*,
                                     ExceptionState&) = 0;
  virtual void clearForBinding(ScriptState*, ExceptionState&) = 0;
  virtual bool deleteForBinding(ScriptState*, FontFace*, ExceptionState&) = 0;
  virtual bool hasForBinding(ScriptState*,
                             FontFace*,
                             ExceptionState&) const = 0;

  virtual size_t size() const = 0;
  virtual AtomicString status() const = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  // Iterable overrides.
  virtual FontFaceSetIterable::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) = 0;

  class IterationSource final : public FontFaceSetIterable::IterationSource {
   public:
    explicit IterationSource(const HeapVector<Member<FontFace>>& font_faces)
        : index_(0), font_faces_(font_faces) {}
    bool Next(ScriptState*,
              Member<FontFace>&,
              Member<FontFace>&,
              ExceptionState&) override;

    DEFINE_INLINE_VIRTUAL_TRACE() {
      visitor->Trace(font_faces_);
      FontFaceSetIterable::IterationSource::Trace(visitor);
    }

   private:
    size_t index_;
    HeapVector<Member<FontFace>> font_faces_;
  };
};

}  // namespace blink

#endif  // FontFaceSet_h
