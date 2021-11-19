// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_

#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}

namespace views {
class View;
class WebView;
class Widget;
}  // namespace views

class GURL;

class ProfilePickerTestBase : public InProcessBrowserTest {
 public:
  ProfilePickerTestBase();
  ~ProfilePickerTestBase() override;

  // Returns the ProfilePickerView that is currently displayed.
  views::View* view();

  // Returns the widget associated with the profile picker.
  views::Widget* widget();

  // Returns the internal web view for the profile picker.
  views::WebView* web_view();

  // Wait until the widget of the picker gets created and the initialization of
  // the picker is thus finished (and notably `widget()` is not null).
  void WaitForPickerWidgetCreated();

  // Waits until `target` WebContents stops loading `url`. If no `target` is
  // provided, it checks for the current `web_contents()` to stop loading `url`.
  // This also works if `web_contents()` changes throughout the waiting as it is
  // technically observing all web contents.
  void WaitForLoadStop(const GURL& url, content::WebContents* target = nullptr);

  // Waits until the picker gets closed.
  void WaitForPickerClosed();
  void WaitForPickerClosedAndReopenedImmediately();

  // Gets the picker's web contents.
  content::WebContents* web_contents();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_
