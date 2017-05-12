// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/WebRemoteFrameBase.h"

#include "core/frame/RemoteFrame.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

WebRemoteFrameBase* WebRemoteFrameBase::FromFrame(RemoteFrame& frame) {
  if (frame.GetPage()) {
    return frame.GetPage()->GetChromeClient().GetWebRemoteFrameBase(frame);
  }
  return nullptr;
}

}  // namespace blink
