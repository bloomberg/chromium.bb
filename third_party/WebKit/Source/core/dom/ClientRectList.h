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

#ifndef ClientRectList_h
#define ClientRectList_h

#include "core/CoreExport.h"
#include "core/dom/ClientRect.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/heap/Handle.h"

namespace blink {

class ClientRect;

class CORE_EXPORT ClientRectList final
    : public GarbageCollected<ClientRectList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ClientRectList* Create() { return new ClientRectList; }
  static ClientRectList* Create(const Vector<FloatQuad>& quads) {
    return new ClientRectList(quads);
  }

  template <typename Rects>
  static ClientRectList* Create(const Rects& rects) {
    return new ClientRectList(rects);
  }

  unsigned length() const;
  ClientRect* item(unsigned index);
  ClientRect* AnonymousIndexedGetter(unsigned index) { return item(index); }

  DECLARE_TRACE();

 private:
  ClientRectList();

  template <typename Rects>
  explicit ClientRectList(const Rects& rects) {
    list_.ReserveInitialCapacity(rects.size());
    for (const auto& r : rects)
      list_.push_back(ClientRect::Create(FloatRect(r)));
  }

  explicit ClientRectList(const Vector<FloatQuad>&);

  HeapVector<Member<ClientRect>> list_;
};

}  // namespace blink

#endif  // ClientRectList_h
