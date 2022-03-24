// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_APP_LIST_BUBBLE_ASSISTANT_PAGE_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_APP_LIST_BUBBLE_ASSISTANT_PAGE_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AppListAssistantMainStage;
class AssistantDialogPlate;
class AssistantViewDelegate;

// The assistant page for the app list bubble / clamshell launcher. Similar to
// AssistantMainView in the fullscreen launcher.
class ASH_EXPORT AppListBubbleAssistantPage : public views::View {
 public:
  METADATA_HEADER(AppListBubbleAssistantPage);

  explicit AppListBubbleAssistantPage(AssistantViewDelegate* delegate);
  AppListBubbleAssistantPage(const AppListBubbleAssistantPage&) = delete;
  AppListBubbleAssistantPage& operator=(const AppListBubbleAssistantPage&) =
      delete;
  ~AppListBubbleAssistantPage() override;

  // views::View:
  void RequestFocus() override;

 private:
  // The text and microphone input area. Owned by views hierarchy.
  AssistantDialogPlate* dialog_plate_;

  // The query and response output area. Owned by views hierarchy.
  AppListAssistantMainStage* main_stage_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_APP_LIST_BUBBLE_ASSISTANT_PAGE_H_
