// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/PageTestBase.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"

namespace blink {

PageTestBase::PageTestBase() {}

PageTestBase::~PageTestBase() {}

void PageTestBase::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

void PageTestBase::SetupPageWithClients(
    Page::PageClients* clients,
    LocalFrameClient* local_frame_client,
    FrameSettingOverrideFunction setting_overrider) {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  dummy_page_holder_ = DummyPageHolder::Create(
      IntSize(800, 600), clients, local_frame_client, setting_overrider);
}

Document& PageTestBase::GetDocument() const {
  return dummy_page_holder_->GetDocument();
}

LocalFrame& PageTestBase::GetFrame() const {
  return GetDummyPageHolder().GetFrame();
}

FrameSelection& PageTestBase::Selection() const {
  return GetFrame().Selection();
}
}  // namespace blink
