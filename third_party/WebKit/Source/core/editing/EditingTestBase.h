// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EditingTestBase_h
#define EditingTestBase_h

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "core/editing/Position.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/Forward.h"

namespace blink {

class FrameSelection;
class LocalFrame;

class EditingTestBase : public ::testing::Test {
  USING_FAST_MALLOC(EditingTestBase);

 protected:
  EditingTestBase();
  ~EditingTestBase() override;

  void SetUp() override;

  void setupPageWithClients(Page::PageClients*);

  Document& document() const;
  DummyPageHolder& dummyPageHolder() const { return *m_dummyPageHolder; }
  LocalFrame& frame() const;
  FrameSelection& selection() const;

  static ShadowRoot* createShadowRootForElementWithIDAndSetInnerHTML(
      TreeScope&,
      const char* hostElementID,
      const char* shadowRootContent);

  void setBodyContent(const std::string&);
  ShadowRoot* setShadowContent(const char* shadowContent,
                               const char* shadowHostId);
  void updateAllLifecyclePhases();

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

}  // namespace blink

#endif  // EditingTestBase_h
