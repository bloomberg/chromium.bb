// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/suggestion/TextSuggestionBackendImpl.h"

#include "core/editing/suggestion/TextSuggestionController.h"
#include "core/frame/LocalFrame.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blink {

TextSuggestionBackendImpl::TextSuggestionBackendImpl(LocalFrame& frame)
    : frame_(frame) {}

// static
void TextSuggestionBackendImpl::Create(
    LocalFrame* frame,
    mojom::blink::TextSuggestionBackendRequest request) {
  mojo::MakeStrongBinding(std::unique_ptr<TextSuggestionBackendImpl>(
                              new TextSuggestionBackendImpl(*frame)),
                          std::move(request));
}

void TextSuggestionBackendImpl::ApplySpellCheckSuggestion(
    const WTF::String& suggestion) {
  if (frame_)
    frame_->GetTextSuggestionController().ApplySpellCheckSuggestion(suggestion);
}

void TextSuggestionBackendImpl::ApplyTextSuggestion(int32_t marker_tag,
                                                    int32_t suggestion_index) {
  if (frame_) {
    frame_->GetTextSuggestionController().ApplyTextSuggestion(marker_tag,
                                                              suggestion_index);
  }
}

void TextSuggestionBackendImpl::DeleteActiveSuggestionRange() {
  if (frame_)
    frame_->GetTextSuggestionController().DeleteActiveSuggestionRange();
}

void TextSuggestionBackendImpl::OnNewWordAddedToDictionary(
    const WTF::String& word) {
  if (frame_)
    frame_->GetTextSuggestionController().OnNewWordAddedToDictionary(word);
}

void TextSuggestionBackendImpl::OnSuggestionMenuClosed() {
  if (frame_)
    frame_->GetTextSuggestionController().OnSuggestionMenuClosed();
}

void TextSuggestionBackendImpl::SuggestionMenuTimeoutCallback(
    int32_t max_number_of_suggestions) {
  if (frame_) {
    frame_->GetTextSuggestionController().SuggestionMenuTimeoutCallback(
        max_number_of_suggestions);
  }
}

}  // namespace blink
