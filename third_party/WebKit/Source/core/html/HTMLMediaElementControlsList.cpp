// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLMediaElementControlsList.h"

#include "core/html/HTMLMediaElement.h"

namespace blink {

namespace {

const char kNoDownload[] = "nodownload";
const char kNoFullscreen[] = "nofullscreen";
const char kNoRemotePlayback[] = "noremoteplayback";

const char* kSupportedTokens[] = {kNoDownload, kNoFullscreen,
                                  kNoRemotePlayback};

}  // namespace

HTMLMediaElementControlsList::HTMLMediaElementControlsList(
    HTMLMediaElement* element)
    : DOMTokenList(this), m_element(element) {}

HTMLMediaElementControlsList::~HTMLMediaElementControlsList() = default;

DEFINE_TRACE(HTMLMediaElementControlsList) {
  visitor->trace(m_element);
  DOMTokenList::trace(visitor);
  DOMTokenListObserver::trace(visitor);
}

bool HTMLMediaElementControlsList::validateTokenValue(
    const AtomicString& tokenValue,
    ExceptionState&) const {
  for (const char* supportedToken : kSupportedTokens) {
    if (tokenValue == supportedToken)
      return true;
  }
  return false;
}

void HTMLMediaElementControlsList::valueWasSet() {
  m_element->controlsListValueWasSet();
}

bool HTMLMediaElementControlsList::shouldHideDownload() const {
  return tokens().contains(kNoDownload);
}

bool HTMLMediaElementControlsList::shouldHideFullscreen() const {
  return tokens().contains(kNoFullscreen);
}

bool HTMLMediaElementControlsList::shouldHideRemotePlayback() const {
  return tokens().contains(kNoRemotePlayback);
}

}  // namespace blink
