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

#ifndef ClientRect_h
#define ClientRect_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/FloatRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class IntRect;

class CORE_EXPORT ClientRect final : public GarbageCollected<ClientRect>,
                                     public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ClientRect* Create() { return new ClientRect; }
  static ClientRect* Create(const IntRect& rect) {
    return new ClientRect(rect);
  }
  static ClientRect* Create(const FloatRect& rect) {
    return new ClientRect(rect);
  }

  float top() const { return rect_.Y(); }
  float right() const { return rect_.MaxX(); }
  float bottom() const { return rect_.MaxY(); }
  float left() const { return rect_.X(); }
  float width() const { return rect_.Width(); }
  float height() const { return rect_.Height(); }

  DEFINE_INLINE_TRACE() {}

 private:
  ClientRect();
  explicit ClientRect(const IntRect&);
  explicit ClientRect(const FloatRect&);

  FloatRect rect_;
};

}  // namespace blink

#endif  // ClientRect_h
