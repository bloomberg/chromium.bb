// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AccessibilityTest_h
#define AccessibilityTest_h

#include <ostream>
#include <sstream>
#include <string>

#include "core/layout/LayoutTestHelper.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class AXObject;
class AXObjectCacheImpl;
class LocalFrameClient;

class AccessibilityTest : public RenderingTest {
  USING_FAST_MALLOC(AccessibilityTest);

 public:
  AccessibilityTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client) {}

 protected:
  void SetUp() override;
  void TearDown() override;

  AXObjectCacheImpl& GetAXObjectCache() const;

  AXObject* GetAXRootObject() const;

  // Returns the object with the accessibility focus.
  AXObject* GetAXFocusedObject() const;

  AXObject* GetAXObjectByElementId(const char* id) const;

  std::string PrintAXTree() const;

 private:
  std::ostringstream& PrintAXTreeHelper(std::ostringstream&,
                                        const AXObject* root,
                                        size_t level) const;
};

}  // namespace blink

#endif  // AccessibilityTest_h
