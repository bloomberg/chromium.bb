// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/RenderedPosition.h"

#include "build/build_config.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/frame/Settings.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/paint/compositing/CompositedSelection.h"

namespace blink {

class RenderedPositionTest : public EditingTestBase {};

#if defined(OS_ANDROID)
#define MAYBE_ComputeCompositedSelection DISABLED_ComputeCompositedSelection
#else
#define MAYBE_ComputeCompositedSelection ComputeCompositedSelection
#endif
TEST_F(RenderedPositionTest, MAYBE_ComputeCompositedSelection) {
  // Enable compositing.
  GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  GetDocument().View()->UpdateAllLifecyclePhases();

  SetBodyContent(
      "<input id=target width=20 value='test test test test test tes tes test'"
      "style='width: 100px; height: 20px;'>");
  HTMLInputElement* target =
      ToHTMLInputElement(GetDocument().getElementById("target"));
  DCHECK(target);
  target->focus();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*target->InnerEditorElement())
          .Build(),
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  UpdateAllLifecyclePhases();

  const CompositedSelection& composited_selection =
      RenderedPosition::ComputeCompositedSelection(Selection());
  EXPECT_FALSE(composited_selection.start.hidden);
  EXPECT_TRUE(composited_selection.end.hidden);
}

}  // namespace blink
