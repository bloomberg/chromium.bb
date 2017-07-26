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
  frame_->GetTextSuggestionController().ApplySpellCheckSuggestion(suggestion);
}

void TextSuggestionBackendImpl::DeleteActiveSuggestionRange() {
  frame_->GetTextSuggestionController().DeleteActiveSuggestionRange();
}

void TextSuggestionBackendImpl::NewWordAddedToDictionary(
    const WTF::String& word) {
  frame_->GetTextSuggestionController().NewWordAddedToDictionary(word);
}

void TextSuggestionBackendImpl::SpellCheckMenuTimeoutCallback() {
  frame_->GetTextSuggestionController().SpellCheckMenuTimeoutCallback();
}

void TextSuggestionBackendImpl::SuggestionMenuClosed() {
  frame_->GetTextSuggestionController().SuggestionMenuClosed();
}

}  // namespace blink
