// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/TextCheckerClientImpl.h"
#include "public/web/WebTextCheckClient.h"
#include "public/web/WebTextCheckingResult.h"
#include "web/WebTextCheckingCompletionImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

TextCheckerClientImpl::TextCheckerClientImpl(WebViewImpl* webView)
    : m_webView(webView) {}

TextCheckerClientImpl::~TextCheckerClientImpl() = default;

void TextCheckerClientImpl::checkSpellingOfString(const String& text,
                                                  int* misspellingLocation,
                                                  int* misspellingLength) {
  // SpellCheckWord will write (0, 0) into the output vars, which is what our
  // caller expects if the word is spelled correctly.
  int spellLocation = -1;
  int spellLength = 0;

  // Check to see if the provided text is spelled correctly.
  if (m_webView->textCheckClient()) {
    m_webView->textCheckClient()->checkSpelling(text, spellLocation,
                                                spellLength, nullptr);
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
  if (!m_webView->textCheckClient())
    return;
  const String& text = request->data().text();
  m_webView->textCheckClient()->requestCheckingOfText(
      text, new WebTextCheckingCompletionImpl(request));
}

void TextCheckerClientImpl::cancelAllPendingRequests() {
  if (!m_webView->textCheckClient())
    return;
  m_webView->textCheckClient()->cancelAllPendingRequests();
}

}  // namespace blink
