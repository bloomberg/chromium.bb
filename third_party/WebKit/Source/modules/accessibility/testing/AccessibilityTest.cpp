// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/testing/AccessibilityTest.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

void AccessibilityTest::SetUp() {
  RenderingTest::SetUp();
  DCHECK(GetDocument().GetSettings());
  GetDocument().GetSettings()->SetAccessibilityEnabled(true);
}

void AccessibilityTest::TearDown() {
  GetDocument().ClearAXObjectCache();
  DCHECK(GetDocument().GetSettings());
  GetDocument().GetSettings()->SetAccessibilityEnabled(false);
  RenderingTest::TearDown();
}

AXObjectCacheImpl& AccessibilityTest::GetAXObjectCache() const {
  auto ax_object_cache = ToAXObjectCacheImpl(GetDocument().AxObjectCache());
  DCHECK(ax_object_cache);
  return *ax_object_cache;
}

AXObject* AccessibilityTest::GetAXRootObject() const {
  return GetAXObjectCache().GetOrCreate(&GetLayoutView());
}

AXObject* AccessibilityTest::GetAXFocusedObject() const {
  return GetAXObjectCache().FocusedObject();
}

AXObject* AccessibilityTest::GetAXObjectByElementId(const char* id) const {
  const auto* element = GetElementById(id);
  return element ? GetAXObjectCache().Get(element) : nullptr;
}

std::string AccessibilityTest::PrintAXTree() const {
  std::ostringstream stream;
  PrintAXTreeHelper(stream, GetAXRootObject(), 0);
  return stream.str();
}

std::ostringstream& AccessibilityTest::PrintAXTreeHelper(
    std::ostringstream& stream,
    const AXObject* root,
    size_t level) const {
  if (!root)
    return stream;

  stream << std::string(level * 2, '+');
  stream << *root << std::endl;
  for (const auto child : const_cast<AXObject*>(root)->Children()) {
    DCHECK(child);
    PrintAXTreeHelper(stream, child.Get(), level + 1);
    stream << std::endl;
  }
  return stream;
}

}  // namespace blink
