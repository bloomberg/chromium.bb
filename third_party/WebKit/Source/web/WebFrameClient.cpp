// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebFrameClient.h"

#include "public/web/WebFrameWidget.h"
#include "public/web/WebLocalFrame.h"

namespace blink {

void WebFrameClient::FrameDetached(WebLocalFrame* frame, DetachType type) {
  if (type == DetachType::kRemove && frame->Parent())
    frame->Parent()->RemoveChild(frame);

  if (frame->FrameWidget())
    frame->FrameWidget()->Close();

  frame->Close();
}

}  // namespace blink
