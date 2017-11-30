// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/FontFaceSetWorker.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/FontFaceSetLoadEvent.h"
#include "core/css/OffscreenFontSelector.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/FontStyleResolver.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/style/ComputedStyle.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

FontFaceSetWorker::FontFaceSetWorker(WorkerGlobalScope& worker)
    : FontFaceSet(worker), Supplement<WorkerGlobalScope>(worker) {
  PauseIfNeeded();
}

FontFaceSetWorker::~FontFaceSetWorker() = default;

WorkerGlobalScope* FontFaceSetWorker::GetWorker() const {
  return ToWorkerGlobalScope(GetExecutionContext());
}

AtomicString FontFaceSetWorker::status() const {
  DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading"));
  DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded"));
  return is_loading_ ? loading : loaded;
}

void FontFaceSetWorker::BeginFontLoading(FontFace* font_face) {
  AddToLoadingFonts(font_face);
}

void FontFaceSetWorker::NotifyLoaded(FontFace* font_face) {
  loaded_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

void FontFaceSetWorker::NotifyError(FontFace* font_face) {
  failed_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

ScriptPromise FontFaceSetWorker::ready(ScriptState* script_state) {
  return ready_->Promise(script_state->World());
}

void FontFaceSetWorker::FireDoneEventIfPossible() {
  if (should_fire_loading_event_)
    return;
  if (!ShouldSignalReady())
    return;

  FireDoneEvent();
}

bool FontFaceSetWorker::ResolveFontStyle(const String& font_string,
                                         Font& font) {
  if (font_string.IsEmpty())
    return false;

  // Interpret fontString in the same way as the 'font' attribute of
  // CanvasRenderingContext2D.
  MutableCSSPropertyValueSet* parsed_style =
      MutableCSSPropertyValueSet::Create(kHTMLStandardMode);
  CSSParser::ParseValue(parsed_style, CSSPropertyFont, font_string, true,
                        GetExecutionContext()->GetSecureContextMode());
  if (parsed_style->IsEmpty())
    return false;

  String font_value = parsed_style->GetPropertyValue(CSSPropertyFont);
  if (font_value == "inherit" || font_value == "initial")
    return false;

  FontFamily font_family;
  font_family.SetFamily(FontFaceSet::kDefaultFontFamily);

  FontDescription default_font_description;
  default_font_description.SetFamily(font_family);
  default_font_description.SetSpecifiedSize(FontFaceSet::kDefaultFontSize);
  default_font_description.SetComputedSize(FontFaceSet::kDefaultFontSize);

  FontDescription description = FontStyleResolver::ComputeFont(
      *parsed_style, GetWorker()->GetFontSelector());

  font = Font(description);
  font.Update(GetWorker()->GetFontSelector());

  return true;
}

FontFaceSetWorker* FontFaceSetWorker::From(WorkerGlobalScope& worker) {
  FontFaceSetWorker* fonts = static_cast<FontFaceSetWorker*>(
      Supplement<WorkerGlobalScope>::From(worker, SupplementName()));
  if (!fonts) {
    fonts = FontFaceSetWorker::Create(worker);
    Supplement<WorkerGlobalScope>::ProvideTo(worker, SupplementName(), fonts);
  }

  return fonts;
}

void FontFaceSetWorker::Trace(Visitor* visitor) {
  Supplement<WorkerGlobalScope>::Trace(visitor);
  FontFaceSet::Trace(visitor);
}

}  // namespace blink
