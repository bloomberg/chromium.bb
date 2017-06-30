// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/LinkManifest.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/html/HTMLLinkElement.h"

namespace blink {

LinkManifest* LinkManifest::Create(HTMLLinkElement* owner) {
  return new LinkManifest(owner);
}

LinkManifest::LinkManifest(HTMLLinkElement* owner) : LinkResource(owner) {}

LinkManifest::~LinkManifest() {}

void LinkManifest::Process() {
  if (!owner_ || !owner_->GetDocument().GetFrame())
    return;

  owner_->GetDocument().GetFrame()->Client()->DispatchDidChangeManifest();
}

bool LinkManifest::HasLoaded() const {
  return false;
}

void LinkManifest::OwnerRemoved() {
  Process();
}

}  // namespace blink
