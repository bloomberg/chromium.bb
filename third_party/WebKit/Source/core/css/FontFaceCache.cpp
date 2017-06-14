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

#include "core/css/FontFaceCache.h"

#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFace.h"
#include "core/css/FontStyleMatcher.h"
#include "core/css/StyleRule.h"
#include "core/loader/resource/FontResource.h"
#include "platform/FontFamilyNames.h"
#include "platform/fonts/FontDescription.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

static unsigned g_version = 0;

FontFaceCache::FontFaceCache() : version_(0) {}

void FontFaceCache::Add(const StyleRuleFontFace* font_face_rule,
                        FontFace* font_face) {
  if (!style_rule_to_font_face_.insert(font_face_rule, font_face).is_new_entry)
    return;
  AddFontFace(font_face, true);
}

void FontFaceCache::AddFontFace(FontFace* font_face, bool css_connected) {
  FamilyToTraitsMap::AddResult traits_result =
      font_faces_.insert(font_face->family(), nullptr);
  if (!traits_result.stored_value->value)
    traits_result.stored_value->value = new TraitsMap;

  TraitsMap::AddResult segmented_font_face_result =
      traits_result.stored_value->value->insert(font_face->Traits().Bitfield(),
                                                nullptr);
  if (!segmented_font_face_result.stored_value->value) {
    segmented_font_face_result.stored_value->value =
        CSSSegmentedFontFace::Create(font_face->Traits());
  }

  segmented_font_face_result.stored_value->value->AddFontFace(font_face,
                                                              css_connected);
  if (css_connected)
    css_connected_font_faces_.insert(font_face);

  fonts_.erase(font_face->family());
  IncrementVersion();
}

void FontFaceCache::Remove(const StyleRuleFontFace* font_face_rule) {
  StyleRuleToFontFace::iterator it =
      style_rule_to_font_face_.find(font_face_rule);
  if (it != style_rule_to_font_face_.end()) {
    RemoveFontFace(it->value.Get(), true);
    style_rule_to_font_face_.erase(it);
  }
}

void FontFaceCache::RemoveFontFace(FontFace* font_face, bool css_connected) {
  FamilyToTraitsMap::iterator font_faces_iter =
      font_faces_.find(font_face->family());
  if (font_faces_iter == font_faces_.end())
    return;
  TraitsMap* family_font_faces = font_faces_iter->value.Get();

  TraitsMap::iterator family_font_faces_iter =
      family_font_faces->find(font_face->Traits().Bitfield());
  if (family_font_faces_iter == family_font_faces->end())
    return;
  CSSSegmentedFontFace* segmented_font_face = family_font_faces_iter->value;

  segmented_font_face->RemoveFontFace(font_face);
  if (segmented_font_face->IsEmpty()) {
    family_font_faces->erase(family_font_faces_iter);
    if (family_font_faces->IsEmpty())
      font_faces_.erase(font_faces_iter);
  }
  fonts_.erase(font_face->family());
  if (css_connected)
    css_connected_font_faces_.erase(font_face);

  IncrementVersion();
}

void FontFaceCache::ClearCSSConnected() {
  for (const auto& item : style_rule_to_font_face_)
    RemoveFontFace(item.value.Get(), true);
  style_rule_to_font_face_.clear();
}

void FontFaceCache::ClearAll() {
  if (font_faces_.IsEmpty())
    return;

  font_faces_.clear();
  fonts_.clear();
  style_rule_to_font_face_.clear();
  css_connected_font_faces_.clear();
  IncrementVersion();
}

void FontFaceCache::IncrementVersion() {
  version_ = ++g_version;
}

CSSSegmentedFontFace* FontFaceCache::Get(
    const FontDescription& font_description,
    const AtomicString& family) {
  TraitsMap* family_font_faces = font_faces_.at(family);
  if (!family_font_faces || family_font_faces->IsEmpty())
    return nullptr;

  FamilyToTraitsMap::AddResult traits_result = fonts_.insert(family, nullptr);
  if (!traits_result.stored_value->value)
    traits_result.stored_value->value = new TraitsMap;

  FontTraits traits = font_description.Traits();
  TraitsMap::AddResult face_result =
      traits_result.stored_value->value->insert(traits.Bitfield(), nullptr);
  if (!face_result.stored_value->value) {
    for (const auto& item : *family_font_faces) {
      CSSSegmentedFontFace* candidate = item.value.Get();
      FontStyleMatcher style_matcher(traits);
      if (!face_result.stored_value->value ||
          style_matcher.IsCandidateBetter(
              candidate, face_result.stored_value->value.Get()))
        face_result.stored_value->value = candidate;
    }
  }
  return face_result.stored_value->value.Get();
}

DEFINE_TRACE(FontFaceCache) {
  visitor->Trace(font_faces_);
  visitor->Trace(fonts_);
  visitor->Trace(style_rule_to_font_face_);
  visitor->Trace(css_connected_font_faces_);
}

}  // namespace blink
