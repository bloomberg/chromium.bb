// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/TextCheckerClientImpl.h"
#include "core/exported/WebTextCheckingCompletionImpl.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "public/web/WebTextCheckClient.h"
#include "public/web/WebTextCheckingResult.h"

namespace blink {

TextCheckerClientImpl::TextCheckerClientImpl(WebLocalFrameImpl* web_local_frame)
    : web_local_frame_(web_local_frame) {}

DEFINE_TRACE(TextCheckerClientImpl) {
  visitor->Trace(web_local_frame_);
}

WebTextCheckClient* TextCheckerClientImpl::GetWebTextCheckClient() const {
  return web_local_frame_->TextCheckClient();
}

void TextCheckerClientImpl::CheckSpellingOfString(const String& text,
                                                  int* misspelling_location,
                                                  int* misspelling_length) {
  // SpellCheckWord will write (0, 0) into the output vars, which is what our
  // caller expects if the word is spelled correctly.
  int spell_location = -1;
  int spell_length = 0;

  // Check to see if the provided text is spelled correctly.
  if (GetWebTextCheckClient()) {
    GetWebTextCheckClient()->CheckSpelling(text, spell_location, spell_length,
                                           nullptr);
  } else {
    spell_location = 0;
    spell_length = 0;
  }

  // Note: the Mac code checks if the pointers are null before writing to them,
  // so we do too.
  if (misspelling_location)
    *misspelling_location = spell_location;
  if (misspelling_length)
    *misspelling_length = spell_length;
}

void TextCheckerClientImpl::RequestCheckingOfString(
    TextCheckingRequest* request) {
  if (!GetWebTextCheckClient())
    return;
  const String& text = request->Data().GetText();
  GetWebTextCheckClient()->RequestCheckingOfText(
      text, new WebTextCheckingCompletionImpl(request));
}

void TextCheckerClientImpl::CancelAllPendingRequests() {
  if (!GetWebTextCheckClient())
    return;
  GetWebTextCheckClient()->CancelAllPendingRequests();
}

}  // namespace blink
