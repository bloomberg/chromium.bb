// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTestHelper_h
#define LayoutTestHelper_h

#include <gtest/gtest.h>
#include <memory>

#include "core/dom/Document.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class SingleChildLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  static SingleChildLocalFrameClient* Create() {
    return new SingleChildLocalFrameClient();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(child_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  // LocalFrameClient overrides:
  LocalFrame* FirstChild() const override { return child_.Get(); }
  LocalFrame* CreateFrame(const AtomicString& name,
                          HTMLFrameOwnerElement*) override;

  void DidDetachChild() { child_ = nullptr; }

 private:
  explicit SingleChildLocalFrameClient() {}

  Member<LocalFrame> child_;
};

class LocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  static LocalFrameClientWithParent* Create(LocalFrame* parent) {
    return new LocalFrameClientWithParent(parent);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  // FrameClient overrides:
  void Detached(FrameDetachType) override;
  LocalFrame* Parent() const override { return parent_.Get(); }

 private:
  explicit LocalFrameClientWithParent(LocalFrame* parent) : parent_(parent) {}

  Member<LocalFrame> parent_;
};

class RenderingTest : public ::testing::Test {
  USING_FAST_MALLOC(RenderingTest);

 public:
  virtual FrameSettingOverrideFunction SettingOverrider() const {
    return nullptr;
  }
  virtual ChromeClient& GetChromeClient() const;

  RenderingTest(LocalFrameClient* = nullptr);

  // Load the 'Ahem' font to the LocalFrame.
  // The 'Ahem' font is the only font whose font metrics is consistent across
  // platforms, but it's not guaranteed to be available.
  // See external/wpt/css/fonts/ahem/README for more about the 'Ahem' font.
  static void LoadAhem(LocalFrame&);

 protected:
  void SetUp() override;
  void TearDown() override;

  Document& GetDocument() const { return page_holder_->GetDocument(); }
  LayoutView& GetLayoutView() const {
    return *ToLayoutView(LayoutAPIShim::LayoutObjectFrom(
        GetDocument().View()->GetLayoutViewItem()));
  }

  // Both sets the inner html and runs the document lifecycle.
  void SetBodyInnerHTML(const String& html_content) {
    GetDocument().body()->setInnerHTML(html_content, ASSERT_NO_EXCEPTION);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  Document& ChildDocument() {
    return *ToLocalFrame(page_holder_->GetFrame().Tree().FirstChild())
                ->GetDocument();
  }

  void SetChildFrameHTML(const String&);

  // Both enables compositing and runs the document lifecycle.
  void EnableCompositing() {
    page_holder_->GetPage().GetSettings().SetAcceleratedCompositingEnabled(
        true);
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  LayoutObject* GetLayoutObjectByElementId(const char* id) const {
    Node* node = GetDocument().getElementById(id);
    return node ? node->GetLayoutObject() : nullptr;
  }

  void LoadAhem();

 private:
  Persistent<LocalFrameClient> local_frame_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

}  // namespace blink

#endif  // LayoutTestHelper_h
