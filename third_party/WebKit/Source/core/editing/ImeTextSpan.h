/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef ImeTextSpan_h
#define ImeTextSpan_h

#include "core/CoreExport.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

struct WebImeTextSpan;

class CORE_EXPORT ImeTextSpan {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum class Type { kComposition, kSuggestion };

  ImeTextSpan(Type,
              unsigned start_offset,
              unsigned end_offset,
              const Color& underline_color,
              bool thick,
              const Color& background_color,
              const Color& suggestion_highlight_color = Color::kTransparent,
              const Vector<String>& suggestions = Vector<String>());

  ImeTextSpan(const WebImeTextSpan&);

  Type GetType() const { return type_; }
  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }
  const Color& UnderlineColor() const { return underline_color_; }
  bool Thick() const { return thick_; }
  const Color& BackgroundColor() const { return background_color_; }
  const Color& SuggestionHighlightColor() const {
    return suggestion_highlight_color_;
  }
  const Vector<String>& Suggestions() const { return suggestions_; }

 private:
  Type type_;
  unsigned start_offset_;
  unsigned end_offset_;
  Color underline_color_;
  bool thick_;
  Color background_color_;
  Color suggestion_highlight_color_;
  Vector<String> suggestions_;
};

}  // namespace blink

#endif  // ImeTextSpan_h
