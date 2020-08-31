// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// InternalPopupMenuTest is not used on Android, and its Platform implementation
// does not provide the resources (as in GetDataResource) needed by
// InternalPopupMenu::WriteDocument.
#if !defined(OS_ANDROID)

TEST(InternalPopupMenuTest, ShowSelectDisplayNone) {
  auto dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder_->GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <div id="container">
      <select id="select">
        <option>1</option>
        <option>2</option>
      </select>
    </div>
  )HTML");
  document.View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  auto* div = document.getElementById("container");
  auto* select = To<HTMLSelectElement>(document.getElementById("select"));
  ASSERT_TRUE(select);
  auto* menu = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *select);

  div->SetInlineStyleProperty(CSSPropertyID::kDisplay, "none");

  // This call should not cause a crash.
  menu->Show();
}

#endif  // defined(OS_ANDROID)

}  // namespace blink
