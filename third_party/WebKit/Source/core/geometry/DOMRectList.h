/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef DOMRectList_h
#define DOMRectList_h

#include "core/CoreExport.h"
#include "core/geometry/DOMRect.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT DOMRectList final : public GarbageCollected<DOMRectList>,
                                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMRectList* Create() { return new DOMRectList; }
  static DOMRectList* Create(const Vector<FloatQuad>& quads) {
    return new DOMRectList(quads);
  }

  template <typename Rects>
  static DOMRectList* Create(const Rects& rects) {
    return new DOMRectList(rects);
  }

  unsigned length() const;
  DOMRect* item(unsigned index);

  DECLARE_TRACE();

 private:
  DOMRectList();

  template <typename Rects>
  explicit DOMRectList(const Rects& rects) {
    list_.ReserveInitialCapacity(rects.size());
    for (const auto& r : rects)
      list_.push_back(DOMRect::FromFloatRect(FloatRect(r)));
  }

  explicit DOMRectList(const Vector<FloatQuad>&);

  HeapVector<Member<DOMRect>> list_;
};

}  // namespace blink

#endif  // DOMRectList_h
