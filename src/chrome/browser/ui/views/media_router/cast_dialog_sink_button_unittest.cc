// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"

namespace media_router {

class CastDialogSinkButtonTest : public ChromeViewsTestBase {
 public:
  CastDialogSinkButtonTest() = default;
  ~CastDialogSinkButtonTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDialogSinkButtonTest);
};

TEST_F(CastDialogSinkButtonTest, SetTitleLabel) {
  UIMediaSink sink;
  sink.friendly_name = base::UTF8ToUTF16("sink name");
  CastDialogSinkButton button(nullptr, sink);
  EXPECT_EQ(sink.friendly_name, button.title()->text());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabel) {
  UIMediaSink sink;

  sink.state = UIMediaSinkState::AVAILABLE;
  CastDialogSinkButton button1(nullptr, sink);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE),
            button1.subtitle()->text());

  sink.state = UIMediaSinkState::CONNECTING;
  CastDialogSinkButton button2(nullptr, sink);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_CONNECTING),
            button2.subtitle()->text());

  sink.status_text = base::UTF8ToUTF16("status text");
  CastDialogSinkButton button3(nullptr, sink);
  EXPECT_EQ(sink.status_text, button3.subtitle()->text());

  sink.issue = Issue(IssueInfo("issue", IssueInfo::Action::DISMISS,
                               IssueInfo::Severity::WARNING));
  CastDialogSinkButton button4(nullptr, sink);
  EXPECT_EQ(base::UTF8ToUTF16(sink.issue->info().title),
            button4.subtitle()->text());
}

}  // namespace media_router
