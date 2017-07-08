// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/NavigatorClipboard.h"

#include "core/clipboard/Clipboard.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"

namespace blink {

Clipboard* NavigatorClipboard::clipboard(ScriptState* script_state,
                                         Navigator& navigator) {
  NavigatorClipboard* supplement = static_cast<NavigatorClipboard*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorClipboard(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }

  return supplement->clipboard_;
}

DEFINE_TRACE(NavigatorClipboard) {
  visitor->Trace(clipboard_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorClipboard::NavigatorClipboard(Navigator& navigator)
    : Supplement<Navigator>(navigator) {
  clipboard_ =
      new Clipboard(GetSupplementable()->GetFrame()
                        ? GetSupplementable()->GetFrame()->GetDocument()
                        : nullptr);
}

const char* NavigatorClipboard::SupplementName() {
  return "NavigatorClipboard";
}

}  // namespace blink
