// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_install_ui_default.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class ExtensionInstalledBubbleViewsBrowserTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionInstalledBubbleViewsBrowserTest()
      : disable_animations_(&ToolbarActionsBar::disable_animations_for_testing_,
                            true) {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kExtensionsToolbarMenu);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kExtensionsToolbarMenu);
    }
  }
  ~ExtensionInstalledBubbleViewsBrowserTest() override = default;

  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 private:
  scoped_refptr<const extensions::Extension> MakeExtensionOfType(
      const std::string& type) {
    extensions::ExtensionBuilder builder(type);

    if (type == "BrowserAction") {
      builder.SetAction(
          extensions::ExtensionBuilder::ActionType::BROWSER_ACTION);
    } else if (type == "PageAction") {
      builder.SetAction(extensions::ExtensionBuilder::ActionType::PAGE_ACTION);
    }

    if (type == "SignInPromo" || type == "NoAction") {
      builder.SetLocation(extensions::Manifest::INTERNAL);
    } else {
      builder.SetLocation(extensions::Manifest::COMPONENT);
    }

    if (type == "Omnibox") {
      auto extra_keys = std::make_unique<base::DictionaryValue>();
      extra_keys->SetString(extensions::manifest_keys::kOmniboxKeyword, "foo");
      builder.MergeManifest(std::move(extra_keys));
    }

    scoped_refptr<const extensions::Extension> extension = builder.Build();
    extension_service()->AddExtension(extension.get());
    return extension;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::AutoReset<bool> disable_animations_;
  views::Widget* bubble_widget_;
};

void ExtensionInstalledBubbleViewsBrowserTest::ShowUi(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension =
      MakeExtensionOfType(name);

  views::Widget::Widgets old_widgets = views::test::WidgetTest::GetAllWidgets();
  ExtensionInstallUIDefault::ShowPlatformBubble(extension, browser(),
                                                SkBitmap());
  views::Widget::Widgets new_widgets = views::test::WidgetTest::GetAllWidgets();
  views::Widget::Widgets added_widgets;
  std::set_difference(new_widgets.begin(), new_widgets.end(),
                      old_widgets.begin(), old_widgets.end(),
                      std::inserter(added_widgets, added_widgets.begin()));
  ASSERT_EQ(added_widgets.size(), 1u);
  bubble_widget_ = *added_widgets.begin();

  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    // With the toolbar menu, the extension slides out of the menu before the
    // bubble shows. Wait for the bubble to become visible.
    views::test::WidgetVisibleWaiter(bubble_widget_).Wait();
  }
}

bool ExtensionInstalledBubbleViewsBrowserTest::VerifyUi() {
  return bubble_widget_->IsVisible();
}

void ExtensionInstalledBubbleViewsBrowserTest::WaitForUserDismissal() {
  views::test::WidgetClosingObserver observer(bubble_widget_);
  observer.Wait();
}

#if defined(OS_CHROMEOS)
// None of these tests work when run under Ash, because they need an
// AuraTestHelper constructed at an inconvenient time in test setup, which
// InProcessBrowserTest is not equipped to handle.
// TODO(ellyjones): Fix that, or figure out an alternate way to test this UI.
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#define MAYBE_InvokeUi_BrowserAction DISABLED_InvokeUi_BrowserAction
#define MAYBE_InvokeUi_PageAction DISABLED_InvokeUi_PageAction
#define MAYBE_InvokeUi_SignInPromo DISABLED_InvokeUi_SignInPromo
#define MAYBE_InvokeUi_Omnibox DISABLED_InvokeUi_Omnibox
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#define MAYBE_InvokeUi_BrowserAction InvokeUi_BrowserAction
#define MAYBE_InvokeUi_PageAction InvokeUi_PageAction
#define MAYBE_InvokeUi_SignInPromo InvokeUi_SignInPromo
#define MAYBE_InvokeUi_Omnibox InvokeUi_Omnibox
#endif

IN_PROC_BROWSER_TEST_P(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_BrowserAction) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_PageAction) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_SignInPromo) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_Omnibox) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionInstalledBubbleViewsBrowserTest,
                         testing::Bool());
