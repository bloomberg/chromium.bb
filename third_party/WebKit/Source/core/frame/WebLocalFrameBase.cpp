// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/WebLocalFrameBase.h"

#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

WebLocalFrameBase* WebLocalFrameBase::FromFrame(LocalFrame* frame) {
  if (frame && frame->GetPage()) {
    return frame->GetPage()->GetChromeClient().GetWebLocalFrameBase(frame);
  }
  return nullptr;
}

WebLocalFrameBase* WebLocalFrameBase::FromFrame(LocalFrame& frame) {
  return WebLocalFrameBase::FromFrame(&frame);
}

}  // namespace blink
