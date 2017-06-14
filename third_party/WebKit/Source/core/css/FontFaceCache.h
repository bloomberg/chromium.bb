/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef FontFaceCache_h
#define FontFaceCache_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class FontFace;
class CSSSegmentedFontFace;
class FontDescription;
class StyleRuleFontFace;

class FontFaceCache final {
  DISALLOW_NEW();

 public:
  FontFaceCache();

  void Add(const StyleRuleFontFace*, FontFace*);
  void Remove(const StyleRuleFontFace*);
  void ClearCSSConnected();
  void ClearAll();
  void AddFontFace(FontFace*, bool css_connected);
  void RemoveFontFace(FontFace*, bool css_connected);

  // FIXME: It's sort of weird that add/remove uses StyleRuleFontFace* as key,
  // but this function uses FontDescription/family pair.
  CSSSegmentedFontFace* Get(const FontDescription&, const AtomicString& family);

  const HeapListHashSet<Member<FontFace>>& CssConnectedFontFaces() const {
    return css_connected_font_faces_;
  }

  unsigned Version() const { return version_; }
  void IncrementVersion();

  DECLARE_TRACE();

 private:
  using TraitsMap = HeapHashMap<unsigned, Member<CSSSegmentedFontFace>>;
  using FamilyToTraitsMap =
      HeapHashMap<String, Member<TraitsMap>, CaseFoldingHash>;
  using StyleRuleToFontFace =
      HeapHashMap<Member<const StyleRuleFontFace>, Member<FontFace>>;
  FamilyToTraitsMap font_faces_;
  FamilyToTraitsMap fonts_;
  StyleRuleToFontFace style_rule_to_font_face_;
  HeapListHashSet<Member<FontFace>> css_connected_font_faces_;

  // FIXME: See if this could be ditched
  // Used to compare Font instances, and the usage seems suspect.
  unsigned version_;
};

}  // namespace blink

#endif
