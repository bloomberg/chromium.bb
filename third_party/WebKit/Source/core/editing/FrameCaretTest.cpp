// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/FrameCaret.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/FocusController.h"
#include "platform/LayoutTestSupport.h"
#include "platform/scheduler/test/fake_web_task_runner.h"

namespace blink {

class FrameCaretTest : public EditingTestBase {
 public:
  FrameCaretTest()
      : m_wasRunningLayoutTest(LayoutTestSupport::isRunningLayoutTest()) {
    // The caret blink timer doesn't work if isRunningLayoutTest() because
    // LayoutTheme::caretBlinkInterval() returns 0.
    LayoutTestSupport::setIsRunningLayoutTest(false);
  }
  ~FrameCaretTest() override {
    LayoutTestSupport::setIsRunningLayoutTest(m_wasRunningLayoutTest);
  }

 private:
  const bool m_wasRunningLayoutTest;
};

TEST_F(FrameCaretTest, BlinkAfterTyping) {
  FrameCaret& caret = selection().frameCaretForTesting();
  RefPtr<scheduler::FakeWebTaskRunner> taskRunner =
      adoptRef(new scheduler::FakeWebTaskRunner);
  taskRunner->setTime(0);
  caret.recreateCaretBlinkTimerForTesting(taskRunner.get());
  const double kInterval = 10;
  LayoutTheme::theme().setCaretBlinkInterval(kInterval);
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);
  document().body()->setInnerHTML("<textarea>");
  Element* editor = toElement(document().body()->firstChild());
  editor->focus();
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(caret.isActive());
  EXPECT_FALSE(caret.shouldShowBlockCursor());
  EXPECT_TRUE(caret.shouldPaintCaretForTesting())
      << "Initially a caret should be in visible cycle.";

  taskRunner->advanceTimeAndRun(kInterval);
  EXPECT_FALSE(caret.shouldPaintCaretForTesting())
      << "The caret blinks normally.";

  TypingCommand::insertLineBreak(document());
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(caret.shouldPaintCaretForTesting())
      << "The caret should be in visible cycle just after a typing command.";

  taskRunner->advanceTimeAndRun(kInterval - 1);
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(caret.shouldPaintCaretForTesting())
      << "The typing command reset the timer. The caret is still visible.";

  taskRunner->advanceTimeAndRun(1);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(caret.shouldPaintCaretForTesting())
      << "The caret should blink after the typing command.";
}

}  // namespace blink
