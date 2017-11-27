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
#include "core/testing/PageTestBase.h"
#include "platform/testing/UseMockScrollbarSettings.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class SingleChildLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  static SingleChildLocalFrameClient* Create() {
    return new SingleChildLocalFrameClient();
  }

  void Trace(blink::Visitor* visitor) override {
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

  void Trace(blink::Visitor* visitor) override {
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

class RenderingTest : public PageTestBase, public UseMockScrollbarSettings {
  USING_FAST_MALLOC(RenderingTest);

 public:
  virtual FrameSettingOverrideFunction SettingOverrider() const {
    return nullptr;
  }
  virtual ChromeClient& GetChromeClient() const;

  explicit RenderingTest(LocalFrameClient* = nullptr);

 protected:
  void SetUp() override;
  void TearDown() override;

  LayoutView& GetLayoutView() const {
    return *ToLayoutView(LayoutAPIShim::LayoutObjectFrom(
        GetDocument().View()->GetLayoutViewItem()));
  }

  Document& ChildDocument() {
    return *ToLocalFrame(GetFrame().Tree().FirstChild())->GetDocument();
  }

  void SetChildFrameHTML(const String&);

  // Both enables compositing and runs the document lifecycle.
  void EnableCompositing() {
    GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  LayoutObject* GetLayoutObjectByElementId(const char* id) const {
    const auto* element = GetElementById(id);
    return element ? element->GetLayoutObject() : nullptr;
  }

 private:
  Persistent<LocalFrameClient> local_frame_client_;
};

}  // namespace blink

#endif  // LayoutTestHelper_h
