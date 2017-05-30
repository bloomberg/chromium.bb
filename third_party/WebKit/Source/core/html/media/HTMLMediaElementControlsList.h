// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementControlsList_h
#define HTMLMediaElementControlsList_h

#include "core/CoreExport.h"
#include "core/dom/DOMTokenList.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLMediaElement;

class HTMLMediaElementControlsList final : public DOMTokenList {
 public:
  static HTMLMediaElementControlsList* Create(HTMLMediaElement* element) {
    return new HTMLMediaElementControlsList(element);
  }

  // Whether the list dictates to hide a certain control.
  CORE_EXPORT bool ShouldHideDownload() const;
  CORE_EXPORT bool ShouldHideFullscreen() const;
  CORE_EXPORT bool ShouldHideRemotePlayback() const;

 private:
  explicit HTMLMediaElementControlsList(HTMLMediaElement*);
  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;
};

}  // namespace blink

#endif
