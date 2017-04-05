// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/TextCheckerClientImpl.h"
#include "public/web/WebTextCheckClient.h"
#include "public/web/WebTextCheckingResult.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebTextCheckingCompletionImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

TextCheckerClientImpl::TextCheckerClientImpl(WebLocalFrameImpl* webLocalFrame)
    : m_webLocalFrame(webLocalFrame) {}

DEFINE_TRACE(TextCheckerClientImpl) {
  visitor->trace(m_webLocalFrame);
}

WebTextCheckClient* TextCheckerClientImpl::webTextCheckClient() const {
  // TODO(xiaochengh): Move WebTextCheckClient to WebLocalFrame.
  return m_webLocalFrame->viewImpl()->textCheckClient();
}

void TextCheckerClientImpl::checkSpellingOfString(const String& text,
                                                  int* misspellingLocation,
                                                  int* misspellingLength) {
  // SpellCheckWord will write (0, 0) into the output vars, which is what our
  // caller expects if the word is spelled correctly.
  int spellLocation = -1;
  int spellLength = 0;

  // Check to see if the provided text is spelled correctly.
  if (webTextCheckClient()) {
    webTextCheckClient()->checkSpelling(text, spellLocation, spellLength,
                                        nullptr);
  } else {
    spellLocation = 0;
    spellLength = 0;
  }

  // Note: the Mac code checks if the pointers are null before writing to them,
  // so we do too.
  if (misspellingLocation)
    *misspellingLocation = spellLocation;
  if (misspellingLength)
    *misspellingLength = spellLength;
}

void TextCheckerClientImpl::requestCheckingOfString(
    TextCheckingRequest* request) {
  if (!webTextCheckClient())
    return;
  const String& text = request->data().text();
  webTextCheckClient()->requestCheckingOfText(
      text, new WebTextCheckingCompletionImpl(request));
}

void TextCheckerClientImpl::cancelAllPendingRequests() {
  if (!webTextCheckClient())
    return;
  webTextCheckClient()->cancelAllPendingRequests();
}

}  // namespace blink
