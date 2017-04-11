// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/HTMLMediaElementControlsList.h"

#include "core/html/HTMLMediaElement.h"

namespace blink {

namespace {

const char kNoDownload[] = "nodownload";
const char kNoFullscreen[] = "nofullscreen";
const char kNoRemotePlayback[] = "noremoteplayback";

const char* const kSupportedTokens[] = {kNoDownload, kNoFullscreen,
                                        kNoRemotePlayback};

}  // namespace

HTMLMediaElementControlsList::HTMLMediaElementControlsList(
    HTMLMediaElement* element)
    : DOMTokenList(this), element_(element) {}

HTMLMediaElementControlsList::~HTMLMediaElementControlsList() = default;

DEFINE_TRACE(HTMLMediaElementControlsList) {
  visitor->Trace(element_);
  DOMTokenList::Trace(visitor);
  DOMTokenListObserver::Trace(visitor);
}

bool HTMLMediaElementControlsList::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  for (const char* supported_token : kSupportedTokens) {
    if (token_value == supported_token)
      return true;
  }
  return false;
}

void HTMLMediaElementControlsList::ValueWasSet() {
  element_->ControlsListValueWasSet();
}

bool HTMLMediaElementControlsList::ShouldHideDownload() const {
  return Tokens().Contains(kNoDownload);
}

bool HTMLMediaElementControlsList::ShouldHideFullscreen() const {
  return Tokens().Contains(kNoFullscreen);
}

bool HTMLMediaElementControlsList::ShouldHideRemotePlayback() const {
  return Tokens().Contains(kNoRemotePlayback);
}

}  // namespace blink
