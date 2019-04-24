// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"

#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"

namespace extensions {

DeclarativeContentConditionTrackerTest::DeclarativeContentConditionTrackerTest()
    : profile_(new TestingProfile),
      next_predicate_group_id_(1) {
}

DeclarativeContentConditionTrackerTest::
~DeclarativeContentConditionTrackerTest() {
  // MockRenderProcessHosts are deleted from the message loop, and their
  // deletion must complete before RenderViewHostTestEnabler's destructor is
  // run.
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<content::WebContents>
DeclarativeContentConditionTrackerTest::MakeTab() {
  std::unique_ptr<content::WebContents> tab(
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr));
  content::RenderFrameHostTester::For(tab->GetMainFrame())
      ->InitializeRenderFrameIfNeeded();
  return tab;
}

content::MockRenderProcessHost*
DeclarativeContentConditionTrackerTest::GetMockRenderProcessHost(
    content::WebContents* contents) {
  return static_cast<content::MockRenderProcessHost*>(
      contents->GetMainFrame()->GetProcess());
}

const void* DeclarativeContentConditionTrackerTest::GeneratePredicateGroupID() {
  // The group ID is opaque to the trackers.
  return reinterpret_cast<const void*>(next_predicate_group_id_++);
}

}  // namespace extensions
