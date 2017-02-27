// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTestHelper_h
#define LayoutTestHelper_h

#include <gtest/gtest.h>
#include <memory>

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/Allocator.h"

namespace blink {

class SingleChildLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  static SingleChildLocalFrameClient* create() {
    return new SingleChildLocalFrameClient();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_child);
    EmptyLocalFrameClient::trace(visitor);
  }

  // LocalFrameClient overrides:
  LocalFrame* firstChild() const override { return m_child.get(); }
  LocalFrame* createFrame(const FrameLoadRequest&,
                          const AtomicString& name,
                          HTMLFrameOwnerElement*) override;

  void didDetachChild() { m_child = nullptr; }

 private:
  explicit SingleChildLocalFrameClient() {}

  Member<LocalFrame> m_child;
};

class LocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  static LocalFrameClientWithParent* create(LocalFrame* parent) {
    return new LocalFrameClientWithParent(parent);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_parent);
    EmptyLocalFrameClient::trace(visitor);
  }

  // FrameClient overrides:
  void detached(FrameDetachType) override;
  LocalFrame* parent() const override { return m_parent.get(); }

 private:
  explicit LocalFrameClientWithParent(LocalFrame* parent) : m_parent(parent) {}

  Member<LocalFrame> m_parent;
};

class RenderingTest : public testing::Test {
  USING_FAST_MALLOC(RenderingTest);

 public:
  virtual FrameSettingOverrideFunction settingOverrider() const {
    return nullptr;
  }
  virtual ChromeClient& chromeClient() const;

  RenderingTest(LocalFrameClient* = nullptr);

 protected:
  void SetUp() override;
  void TearDown() override;

  Document& document() const { return m_pageHolder->document(); }
  LayoutView& layoutView() const {
    return *toLayoutView(
        LayoutAPIShim::layoutObjectFrom(document().view()->layoutViewItem()));
  }

  // Both sets the inner html and runs the document lifecycle.
  void setBodyInnerHTML(const String& htmlContent) {
    document().body()->setInnerHTML(htmlContent, ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
  }

  Document& childDocument() {
    return *toLocalFrame(m_pageHolder->frame().tree().firstChild())->document();
  }

  void setChildFrameHTML(const String&);

  // Both enables compositing and runs the document lifecycle.
  void enableCompositing() {
    m_pageHolder->page().settings().setAcceleratedCompositingEnabled(true);
    document().view()->setParentVisible(true);
    document().view()->setSelfVisible(true);
    document().view()->updateAllLifecyclePhases();
  }

  LayoutObject* getLayoutObjectByElementId(const char* id) const {
    Node* node = document().getElementById(id);
    return node ? node->layoutObject() : nullptr;
  }

  void loadAhem();

 private:
  Persistent<LocalFrameClient> m_localFrameClient;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

}  // namespace blink

#endif  // LayoutTestHelper_h
