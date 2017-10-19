// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/NavigatorUserMedia.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/mediastream/MediaDevices.h"

namespace blink {

NavigatorUserMedia::NavigatorUserMedia(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      media_devices_(MediaDevices::Create(
          navigator.GetFrame() ? navigator.GetFrame()->GetDocument()
                               : nullptr)) {}

const char* NavigatorUserMedia::SupplementName() {
  return "NavigatorUserMedia";
}

NavigatorUserMedia& NavigatorUserMedia::From(Navigator& navigator) {
  NavigatorUserMedia* supplement = static_cast<NavigatorUserMedia*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorUserMedia(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

MediaDevices* NavigatorUserMedia::GetMediaDevices() {
  return media_devices_;
}

MediaDevices* NavigatorUserMedia::mediaDevices(Navigator& navigator) {
  return NavigatorUserMedia::From(navigator).GetMediaDevices();
}

void NavigatorUserMedia::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_devices_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
