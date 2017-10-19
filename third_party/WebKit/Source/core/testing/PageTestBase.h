// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PageTestBase_h
#define PageTestBase_h

#include <gtest/gtest.h>
#include "core/testing/DummyPageHolder.h"

namespace blink {

class Document;
class LocalFrame;

class PageTestBase : public ::testing::Test {
  USING_FAST_MALLOC(PageTestBase);

 public:
  PageTestBase();
  ~PageTestBase();

  void SetUp() override;
  void TearDown() override;

  void SetupPageWithClients(Page::PageClients* = 0,
                            LocalFrameClient* = nullptr,
                            FrameSettingOverrideFunction = nullptr);
  Document& GetDocument() const;
  Page& GetPage() const;
  LocalFrame& GetFrame() const;
  FrameSelection& Selection() const;
  DummyPageHolder& GetDummyPageHolder() const { return *dummy_page_holder_; }

  // Load the 'Ahem' font to the LocalFrame.
  // The 'Ahem' font is the only font whose font metrics is consistent across
  // platforms, but it's not guaranteed to be available.
  // See external/wpt/css/fonts/ahem/README for more about the 'Ahem' font.
  static void LoadAhem(LocalFrame&);

 protected:
  void LoadAhem();

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

}  // namespace blink

#endif  // PageTestBase_h
