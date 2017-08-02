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

#include "core/css/FontFaceSet.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/FontFaceSetLoadEvent.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/style/ComputedStyle.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

static const int kDefaultFontSize = 10;
static const char kDefaultFontFamily[] = "sans-serif";

class LoadFontPromiseResolver final
    : public GarbageCollectedFinalized<LoadFontPromiseResolver>,
      public FontFace::LoadFontCallback {
  USING_GARBAGE_COLLECTED_MIXIN(LoadFontPromiseResolver);

 public:
  static LoadFontPromiseResolver* Create(FontFaceArray faces,
                                         ScriptState* script_state) {
    return new LoadFontPromiseResolver(faces, script_state);
  }

  void LoadFonts();
  ScriptPromise Promise() { return resolver_->Promise(); }

  void NotifyLoaded(FontFace*) override;
  void NotifyError(FontFace*) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  LoadFontPromiseResolver(FontFaceArray faces, ScriptState* script_state)
      : num_loading_(faces.size()),
        error_occured_(false),
        resolver_(ScriptPromiseResolver::Create(script_state)) {
    font_faces_.swap(faces);
  }

  HeapVector<Member<FontFace>> font_faces_;
  int num_loading_;
  bool error_occured_;
  Member<ScriptPromiseResolver> resolver_;
};

void LoadFontPromiseResolver::LoadFonts() {
  if (!num_loading_) {
    resolver_->Resolve(font_faces_);
    return;
  }

  for (size_t i = 0; i < font_faces_.size(); i++)
    font_faces_[i]->LoadWithCallback(this);
}

void LoadFontPromiseResolver::NotifyLoaded(FontFace* font_face) {
  num_loading_--;
  if (num_loading_ || error_occured_)
    return;

  resolver_->Resolve(font_faces_);
}

void LoadFontPromiseResolver::NotifyError(FontFace* font_face) {
  num_loading_--;
  if (!error_occured_) {
    error_occured_ = true;
    resolver_->Reject(font_face->GetError());
  }
}

DEFINE_TRACE(LoadFontPromiseResolver) {
  visitor->Trace(font_faces_);
  visitor->Trace(resolver_);
  LoadFontCallback::Trace(visitor);
}

FontFaceSet::FontFaceSet(Document& document)
    : Supplement<Document>(document),
      SuspendableObject(&document),
      should_fire_loading_event_(false),
      is_loading_(false),
      ready_(new ReadyProperty(GetExecutionContext(),
                               this,
                               ReadyProperty::kReady)),
      async_runner_(AsyncMethodRunner<FontFaceSet>::Create(
          this,
          &FontFaceSet::HandlePendingEventsAndPromises)) {
  SuspendIfNeeded();
}

FontFaceSet::~FontFaceSet() {}

Document* FontFaceSet::GetDocument() const {
  return ToDocument(GetExecutionContext());
}

bool FontFaceSet::InActiveDocumentContext() const {
  ExecutionContext* context = GetExecutionContext();
  return context && ToDocument(context)->IsActive();
}

void FontFaceSet::AddFontFacesToFontFaceCache(FontFaceCache* font_face_cache) {
  for (const auto& font_face : non_css_connected_faces_)
    font_face_cache->AddFontFace(font_face, false);
}

const AtomicString& FontFaceSet::InterfaceName() const {
  return EventTargetNames::FontFaceSet;
}

ExecutionContext* FontFaceSet::GetExecutionContext() const {
  return SuspendableObject::GetExecutionContext();
}

AtomicString FontFaceSet::status() const {
  DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading"));
  DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded"));
  return is_loading_ ? loading : loaded;
}

void FontFaceSet::HandlePendingEventsAndPromisesSoon() {
  // async_runner_ will be automatically stopped on destruction.
  async_runner_->RunAsync();
}

void FontFaceSet::DidLayout() {
  if (GetDocument()->GetFrame()->IsMainFrame() && loading_fonts_.IsEmpty())
    histogram_.Record();
  if (!ShouldSignalReady())
    return;
  HandlePendingEventsAndPromisesSoon();
}

bool FontFaceSet::ShouldSignalReady() const {
  if (!loading_fonts_.IsEmpty())
    return false;
  return is_loading_ || ready_->GetState() == ReadyProperty::kPending;
}

void FontFaceSet::HandlePendingEventsAndPromises() {
  FireLoadingEvent();
  FireDoneEventIfPossible();
}

void FontFaceSet::FireLoadingEvent() {
  if (should_fire_loading_event_) {
    should_fire_loading_event_ = false;
    DispatchEvent(
        FontFaceSetLoadEvent::CreateForFontFaces(EventTypeNames::loading));
  }
}

void FontFaceSet::Suspend() {
  async_runner_->Suspend();
}

void FontFaceSet::Resume() {
  async_runner_->Resume();
}

void FontFaceSet::ContextDestroyed(ExecutionContext*) {
  async_runner_->Stop();
}

void FontFaceSet::BeginFontLoading(FontFace* font_face) {
  histogram_.IncrementCount();
  AddToLoadingFonts(font_face);
}

void FontFaceSet::NotifyLoaded(FontFace* font_face) {
  histogram_.UpdateStatus(font_face);
  loaded_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

void FontFaceSet::NotifyError(FontFace* font_face) {
  histogram_.UpdateStatus(font_face);
  failed_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

size_t FontFaceSet::ApproximateBlankCharacterCount() const {
  size_t count = 0;
  for (auto& font_face : loading_fonts_)
    count += font_face->ApproximateBlankCharacterCount();
  return count;
}

void FontFaceSet::AddToLoadingFonts(FontFace* font_face) {
  if (!is_loading_) {
    is_loading_ = true;
    should_fire_loading_event_ = true;
    if (ready_->GetState() != ReadyProperty::kPending)
      ready_->Reset();
    HandlePendingEventsAndPromisesSoon();
  }
  loading_fonts_.insert(font_face);
  font_face->AddCallback(this);
}

void FontFaceSet::RemoveFromLoadingFonts(FontFace* font_face) {
  loading_fonts_.erase(font_face);
  if (loading_fonts_.IsEmpty())
    HandlePendingEventsAndPromisesSoon();
}

ScriptPromise FontFaceSet::ready(ScriptState* script_state) {
  if (ready_->GetState() != ReadyProperty::kPending &&
      InActiveDocumentContext()) {
    // |ready_| is already resolved, but there may be pending stylesheet
    // changes and/or layout operations that may cause another font loads.
    // So synchronously update style and layout here.
    // This may trigger font loads, and replace |ready_| with a new Promise.
    GetDocument()->UpdateStyleAndLayout();
  }
  return ready_->Promise(script_state->World());
}

FontFaceSet* FontFaceSet::addForBinding(ScriptState*,
                                        FontFace* font_face,
                                        ExceptionState&) {
  DCHECK(font_face);
  if (!InActiveDocumentContext())
    return this;
  if (non_css_connected_faces_.Contains(font_face))
    return this;
  if (IsCSSConnectedFontFace(font_face))
    return this;
  CSSFontSelector* font_selector =
      GetDocument()->GetStyleEngine().GetFontSelector();
  non_css_connected_faces_.insert(font_face);
  font_selector->GetFontFaceCache()->AddFontFace(font_face, false);
  if (font_face->LoadStatus() == FontFace::kLoading)
    AddToLoadingFonts(font_face);
  font_selector->FontFaceInvalidated();
  return this;
}

void FontFaceSet::clearForBinding(ScriptState*, ExceptionState&) {
  if (!InActiveDocumentContext() || non_css_connected_faces_.IsEmpty())
    return;
  CSSFontSelector* font_selector =
      GetDocument()->GetStyleEngine().GetFontSelector();
  FontFaceCache* font_face_cache = font_selector->GetFontFaceCache();
  for (const auto& font_face : non_css_connected_faces_) {
    font_face_cache->RemoveFontFace(font_face.Get(), false);
    if (font_face->LoadStatus() == FontFace::kLoading)
      RemoveFromLoadingFonts(font_face);
  }
  non_css_connected_faces_.clear();
  font_selector->FontFaceInvalidated();
}

bool FontFaceSet::deleteForBinding(ScriptState*,
                                   FontFace* font_face,
                                   ExceptionState&) {
  DCHECK(font_face);
  if (!InActiveDocumentContext())
    return false;
  HeapListHashSet<Member<FontFace>>::iterator it =
      non_css_connected_faces_.find(font_face);
  if (it != non_css_connected_faces_.end()) {
    non_css_connected_faces_.erase(it);
    CSSFontSelector* font_selector =
        GetDocument()->GetStyleEngine().GetFontSelector();
    font_selector->GetFontFaceCache()->RemoveFontFace(font_face, false);
    if (font_face->LoadStatus() == FontFace::kLoading)
      RemoveFromLoadingFonts(font_face);
    font_selector->FontFaceInvalidated();
    return true;
  }
  return false;
}

bool FontFaceSet::hasForBinding(ScriptState*,
                                FontFace* font_face,
                                ExceptionState&) const {
  DCHECK(font_face);
  if (!InActiveDocumentContext())
    return false;
  return non_css_connected_faces_.Contains(font_face) ||
         IsCSSConnectedFontFace(font_face);
}

const HeapListHashSet<Member<FontFace>>& FontFaceSet::CssConnectedFontFaceList()
    const {
  Document* document = this->GetDocument();
  document->UpdateActiveStyle();
  return document->GetStyleEngine()
      .GetFontSelector()
      ->GetFontFaceCache()
      ->CssConnectedFontFaces();
}

bool FontFaceSet::IsCSSConnectedFontFace(FontFace* font_face) const {
  return CssConnectedFontFaceList().Contains(font_face);
}

size_t FontFaceSet::size() const {
  if (!InActiveDocumentContext())
    return non_css_connected_faces_.size();
  return CssConnectedFontFaceList().size() + non_css_connected_faces_.size();
}

void FontFaceSet::FireDoneEventIfPossible() {
  if (should_fire_loading_event_)
    return;
  if (!ShouldSignalReady())
    return;
  Document* d = GetDocument();
  if (!d)
    return;

  // If the layout was invalidated in between when we thought layout
  // was updated and when we're ready to fire the event, just wait
  // until after the next layout before firing events.
  if (!d->View() || d->View()->NeedsLayout())
    return;

  if (is_loading_) {
    FontFaceSetLoadEvent* done_event = nullptr;
    FontFaceSetLoadEvent* error_event = nullptr;
    done_event = FontFaceSetLoadEvent::CreateForFontFaces(
        EventTypeNames::loadingdone, loaded_fonts_);
    loaded_fonts_.clear();
    if (!failed_fonts_.IsEmpty()) {
      error_event = FontFaceSetLoadEvent::CreateForFontFaces(
          EventTypeNames::loadingerror, failed_fonts_);
      failed_fonts_.clear();
    }
    is_loading_ = false;
    DispatchEvent(done_event);
    if (error_event)
      DispatchEvent(error_event);
  }

  if (ready_->GetState() == ReadyProperty::kPending)
    ready_->Resolve(this);
}

ScriptPromise FontFaceSet::load(ScriptState* script_state,
                                const String& font_string,
                                const String& text) {
  if (!InActiveDocumentContext())
    return ScriptPromise();

  Font font;
  if (!ResolveFontStyle(font_string, font)) {
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::Create(script_state);
    ScriptPromise promise = resolver->Promise();
    resolver->Reject(DOMException::Create(
        kSyntaxError, "Could not resolve '" + font_string + "' as a font."));
    return promise;
  }

  FontFaceCache* font_face_cache =
      GetDocument()->GetStyleEngine().GetFontSelector()->GetFontFaceCache();
  FontFaceArray faces;
  for (const FontFamily* f = &font.GetFontDescription().Family(); f;
       f = f->Next()) {
    CSSSegmentedFontFace* segmented_font_face =
        font_face_cache->Get(font.GetFontDescription(), f->Family());
    if (segmented_font_face)
      segmented_font_face->Match(text, faces);
  }

  LoadFontPromiseResolver* resolver =
      LoadFontPromiseResolver::Create(faces, script_state);
  ScriptPromise promise = resolver->Promise();
  // After this, resolver->promise() may return null.
  resolver->LoadFonts();
  return promise;
}

bool FontFaceSet::check(const String& font_string,
                        const String& text,
                        ExceptionState& exception_state) {
  if (!InActiveDocumentContext())
    return false;

  Font font;
  if (!ResolveFontStyle(font_string, font)) {
    exception_state.ThrowDOMException(
        kSyntaxError, "Could not resolve '" + font_string + "' as a font.");
    return false;
  }

  CSSFontSelector* font_selector =
      GetDocument()->GetStyleEngine().GetFontSelector();
  FontFaceCache* font_face_cache = font_selector->GetFontFaceCache();

  bool has_loaded_faces = false;
  for (const FontFamily* f = &font.GetFontDescription().Family(); f;
       f = f->Next()) {
    CSSSegmentedFontFace* face =
        font_face_cache->Get(font.GetFontDescription(), f->Family());
    if (face) {
      if (!face->CheckFont(text))
        return false;
      has_loaded_faces = true;
    }
  }
  if (has_loaded_faces)
    return true;
  for (const FontFamily* f = &font.GetFontDescription().Family(); f;
       f = f->Next()) {
    if (font_selector->IsPlatformFamilyMatchAvailable(font.GetFontDescription(),
                                                      f->Family()))
      return true;
  }
  return false;
}

bool FontFaceSet::ResolveFontStyle(const String& font_string, Font& font) {
  if (font_string.IsEmpty())
    return false;

  // Interpret fontString in the same way as the 'font' attribute of
  // CanvasRenderingContext2D.
  MutableStylePropertySet* parsed_style =
      MutableStylePropertySet::Create(kHTMLStandardMode);
  CSSParser::ParseValue(parsed_style, CSSPropertyFont, font_string, true);
  if (parsed_style->IsEmpty())
    return false;

  String font_value = parsed_style->GetPropertyValue(CSSPropertyFont);
  if (font_value == "inherit" || font_value == "initial")
    return false;

  RefPtr<ComputedStyle> style = ComputedStyle::Create();

  FontFamily font_family;
  font_family.SetFamily(kDefaultFontFamily);

  FontDescription default_font_description;
  default_font_description.SetFamily(font_family);
  default_font_description.SetSpecifiedSize(kDefaultFontSize);
  default_font_description.SetComputedSize(kDefaultFontSize);

  style->SetFontDescription(default_font_description);

  style->GetFont().Update(style->GetFont().GetFontSelector());

  GetDocument()->UpdateActiveStyle();
  GetDocument()->EnsureStyleResolver().ComputeFont(style.Get(), *parsed_style);

  font = style->GetFont();
  font.Update(GetDocument()->GetStyleEngine().GetFontSelector());
  return true;
}

void FontFaceSet::FontLoadHistogram::UpdateStatus(FontFace* font_face) {
  if (status_ == kReported)
    return;
  if (font_face->HadBlankText())
    status_ = kHadBlankText;
  else if (status_ == kNoWebFonts)
    status_ = kDidNotHaveBlankText;
}

void FontFaceSet::FontLoadHistogram::Record() {
  if (!recorded_) {
    recorded_ = true;
    DEFINE_STATIC_LOCAL(CustomCountHistogram, web_fonts_in_page_histogram,
                        ("WebFont.WebFontsInPage", 1, 100, 50));
    web_fonts_in_page_histogram.Count(count_);
  }
  if (status_ == kHadBlankText || status_ == kDidNotHaveBlankText) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, had_blank_text_histogram,
                        ("WebFont.HadBlankText", 2));
    had_blank_text_histogram.Count(status_ == kHadBlankText ? 1 : 0);
    status_ = kReported;
  }
}

FontFaceSet* FontFaceSet::From(Document& document) {
  FontFaceSet* fonts = static_cast<FontFaceSet*>(
      Supplement<Document>::From(document, SupplementName()));
  if (!fonts) {
    fonts = FontFaceSet::Create(document);
    Supplement<Document>::ProvideTo(document, SupplementName(), fonts);
  }

  return fonts;
}

void FontFaceSet::DidLayout(Document& document) {
  if (FontFaceSet* fonts = static_cast<FontFaceSet*>(
          Supplement<Document>::From(document, SupplementName())))
    fonts->DidLayout();
}

size_t FontFaceSet::ApproximateBlankCharacterCount(Document& document) {
  if (FontFaceSet* fonts = static_cast<FontFaceSet*>(
          Supplement<Document>::From(document, SupplementName())))
    return fonts->ApproximateBlankCharacterCount();
  return 0;
}

FontFaceSetIterable::IterationSource* FontFaceSet::StartIteration(
    ScriptState*,
    ExceptionState&) {
  // Setlike should iterate each item in insertion order, and items should
  // be keep on up to date. But since blink does not have a way to hook up CSS
  // modification, take a snapshot here, and make it ordered as follows.
  HeapVector<Member<FontFace>> font_faces;
  if (InActiveDocumentContext()) {
    const HeapListHashSet<Member<FontFace>>& css_connected_faces =
        CssConnectedFontFaceList();
    font_faces.ReserveInitialCapacity(css_connected_faces.size() +
                                      non_css_connected_faces_.size());
    for (const auto& font_face : css_connected_faces)
      font_faces.push_back(font_face);
    for (const auto& font_face : non_css_connected_faces_)
      font_faces.push_back(font_face);
  }
  return new IterationSource(font_faces);
}

bool FontFaceSet::IterationSource::Next(ScriptState*,
                                        Member<FontFace>& key,
                                        Member<FontFace>& value,
                                        ExceptionState&) {
  if (font_faces_.size() <= index_)
    return false;
  key = value = font_faces_[index_++];
  return true;
}

DEFINE_TRACE(FontFaceSet) {
  visitor->Trace(ready_);
  visitor->Trace(loading_fonts_);
  visitor->Trace(loaded_fonts_);
  visitor->Trace(failed_fonts_);
  visitor->Trace(non_css_connected_faces_);
  visitor->Trace(async_runner_);
  EventTargetWithInlineData::Trace(visitor);
  Supplement<Document>::Trace(visitor);
  SuspendableObject::Trace(visitor);
  FontFace::LoadFontCallback::Trace(visitor);
}

}  // namespace blink
