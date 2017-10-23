/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 */

#ifndef CanvasPattern_h
#define CanvasPattern_h

#include "core/svg/SVGMatrixTearOff.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/graphics/Pattern.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ExceptionState;
class Image;

class CanvasPattern final : public GarbageCollectedFinalized<CanvasPattern>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Pattern::RepeatMode ParseRepetitionType(const String&,
                                                 ExceptionState&);

  static CanvasPattern* Create(scoped_refptr<Image> image,
                               Pattern::RepeatMode repeat,
                               bool origin_clean) {
    return new CanvasPattern(std::move(image), repeat, origin_clean);
  }

  Pattern* GetPattern() const { return pattern_.get(); }
  const AffineTransform& GetTransform() const { return pattern_transform_; }

  bool OriginClean() const { return origin_clean_; }

  void Trace(blink::Visitor* visitor) {}

  void setTransform(SVGMatrixTearOff*);

 private:
  CanvasPattern(scoped_refptr<Image>, Pattern::RepeatMode, bool origin_clean);

  scoped_refptr<Pattern> pattern_;
  AffineTransform pattern_transform_;
  bool origin_clean_;
};

}  // namespace blink

#endif  // CanvasPattern_h
