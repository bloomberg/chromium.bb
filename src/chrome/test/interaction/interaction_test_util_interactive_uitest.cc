// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_test_util.h"

#include "base/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/element_tracker_views.h"

// This allows us to test features of InteractionTestUtil that only work in a
// single-process production environment - specifically, ensuring it works with
// context menus, which are native menus on Mac.
class InteractionTestUtilInteractiveUitest : public InProcessBrowserTest {
 public:
  InteractionTestUtilInteractiveUitest() = default;
  ~InteractionTestUtilInteractiveUitest() override = default;

 protected:
  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  ui::ElementContext GetContext() {
    return browser()->window()->GetElementContext();
  }
};

// We only use InputType::kDefault (the default) when testing context menus
// since input injection on Mac is unavailable/unreliable for specific input
// modes, but kDefault should be guaranteed to work.
IN_PROC_BROWSER_TEST_F(InteractionTestUtilInteractiveUitest,
                       SelectContextMenuItem) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Tab* const tab = GetBrowserView()->tabstrip()->tab_at(0);

  // Because context menus run synchronously on Mac we will use an
  // InteractionSequence in order to show the context menu, respond immediately
  // when the menu is shown, and then tear down the menu (by closing the
  // browser) when we're done.
  //
  // Interaction sequences let us respond synchronously to events as well as
  // queue up sequences of actions in response to UI changes in a way that
  // would be far more complicated trying to use RunLoops, tasks, and events.

  auto open_context_menu = base::BindLambdaForTesting([&]() {
    tab->ShowContextMenu(tab->bounds().CenterPoint(),
                         ui::MenuSourceType::MENU_SOURCE_MOUSE);
  });

  auto set_up = base::BindLambdaForTesting(
      [&](ui::InteractionSequence*, ui::TrackedElement*) {
#if BUILDFLAG(IS_MAC)
        // Have to defer opening because this call is blocking on Mac;
        // subsequent steps will be called from within the run loop of the
        // context menu.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, std::move(open_context_menu));
#else
        // Conversely, on other platforms, this is already an async call and
        // weirdness sometimes results from doing this from a callback, so
        // we'll call it directly instead.
        open_context_menu.Run();
#endif
      });

  auto click_menu_item =
      base::BindOnce([](ui::InteractionSequence*, ui::TrackedElement* element) {
        auto util = CreateInteractionTestUtil();
        util->SelectMenuItem(element);
      });

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(ui::InteractionSequence::WithInitialElement(
              views::ElementTrackerViews::GetInstance()->GetElementForView(
                  tab, /* assign_temporary_id =*/true),
              set_up))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TabMenuModel::kAddToNewGroupItemIdentifier)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(std::move(click_menu_item))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TabMenuModel::kAddToNewGroupItemIdentifier)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TabMenuModel::kAddToNewGroupItemIdentifier)
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
