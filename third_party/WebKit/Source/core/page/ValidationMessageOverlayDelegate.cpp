// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ValidationMessageOverlayDelegate.h"

namespace blink {

inline ValidationMessageOverlayDelegate::ValidationMessageOverlayDelegate(
    WebViewBase& web_view,
    const Element& anchor,
    const String& message,
    TextDirection message_dir,
    const String& sub_message,
    TextDirection sub_message_dir) {}

std::unique_ptr<ValidationMessageOverlayDelegate>
ValidationMessageOverlayDelegate::Create(WebViewBase& web_view,
                                         const Element& anchor,
                                         const String& message,
                                         TextDirection message_dir,
                                         const String& sub_message,
                                         TextDirection sub_message_dir) {
  return WTF::WrapUnique(new ValidationMessageOverlayDelegate(
      web_view, anchor, message, message_dir, sub_message, sub_message_dir));
}

void ValidationMessageOverlayDelegate::PaintPageOverlay(
    const PageOverlay& overlay,
    GraphicsContext& context,
    const WebSize& view_size) const {
  // TODO(tkent): Implement this.
}

}  // namespace blink
