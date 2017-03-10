// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementControlsList_h
#define HTMLMediaElementControlsList_h

#include "core/dom/DOMTokenList.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLMediaElement;

class HTMLMediaElementControlsList final : public DOMTokenList,
                                           public DOMTokenListObserver {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementControlsList);

 public:
  static HTMLMediaElementControlsList* create(HTMLMediaElement* element) {
    return new HTMLMediaElementControlsList(element);
  }

  ~HTMLMediaElementControlsList() override;

  DECLARE_VIRTUAL_TRACE();

  // Whether the list dictates to hide a certain control.
  bool shouldHideDownload() const;
  bool shouldHideFullscreen() const;
  bool shouldHideRemotePlayback() const;

 private:
  explicit HTMLMediaElementControlsList(HTMLMediaElement*);
  bool validateTokenValue(const AtomicString&, ExceptionState&) const override;

  // DOMTokenListObserver.
  void valueWasSet() override;

  Member<HTMLMediaElement> m_element;
};

}  // namespace blink

#endif
