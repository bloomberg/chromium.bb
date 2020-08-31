// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/insecure_sensitive_input_driver.h"

#include <memory>
#include <string>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

namespace {

class InsecureSensitiveInputDriverTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  InsecureSensitiveInputDriverTest() {}

  ~InsecureSensitiveInputDriverTest() override {}

  security_state::InsecureInputEventData GetInputEvents(
      content::NavigationEntry* entry) {
    security_state::SSLStatusInputEventData* input_events =
        static_cast<security_state::SSLStatusInputEventData*>(
            entry->GetSSL().user_data.get());
    if (input_events)
      return *input_events->input_events();

    return security_state::InsecureInputEventData();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InsecureSensitiveInputDriverTest);
};

// Tests that field edit notifications are forwarded.
TEST_F(InsecureSensitiveInputDriverTest, FieldEdit) {
  std::unique_ptr<InsecureSensitiveInputDriver> driver(
      new InsecureSensitiveInputDriver(main_rfh()));

  // Do a mock navigation so that there is a navigation entry on which the
  // field edit gets recorded.
  GURL url("http://example.test");
  NavigateAndCommit(url);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(GetInputEvents(entry).insecure_field_edited);

  driver->DidEditFieldInInsecureContext();

  // Check that the field edit notification was passed on.
  entry = web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(GetInputEvents(entry).insecure_field_edited);
}

// Tests that field edit notifications from subframes are
// recorded correctly.
TEST_F(InsecureSensitiveInputDriverTest, FieldEditWithSubframe) {
  // Do a mock navigation so that there is a navigation entry on which
  // field edit modifications get recorded.
  GURL url("http://example.test");
  NavigateAndCommit(url);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(GetInputEvents(entry).insecure_field_edited);

  // Create a subframe and check that notifications for field edits
  // are handled properly.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://example2.test"), subframe);
  auto subframe_driver =
      std::make_unique<InsecureSensitiveInputDriver>(subframe);
  subframe_driver->DidEditFieldInInsecureContext();

  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(GetInputEvents(entry).insecure_field_edited);
}

}  // namespace
