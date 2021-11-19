// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sharesheet/copy_to_clipboard_share_action.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {
namespace sharesheet {

CopyToClipboardShareAction::CopyToClipboardShareAction(Profile* profile)
    : profile_(profile) {}

CopyToClipboardShareAction::~CopyToClipboardShareAction() = default;

const std::u16string CopyToClipboardShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(
      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL);
}

const gfx::VectorIcon& CopyToClipboardShareAction::GetActionIcon() {
  return kCopyIcon;
}

void CopyToClipboardShareAction::LaunchAction(
    ::sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::mojom::IntentPtr intent) {
  controller_ = controller;
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);

  if (intent->share_text.has_value()) {
    apps_util::SharedText extracted_text =
        apps_util::ExtractSharedText(intent->share_text.value());

    if (!extracted_text.text.empty()) {
      clipboard_writer.WriteText(base::UTF8ToUTF16(extracted_text.text));
    }

    if (!extracted_text.url.is_empty()) {
      std::string anchor_text;
      if (intent->share_title.has_value() &&
          !(intent->share_title.value().empty())) {
        anchor_text = intent->share_title.value();
      }
      clipboard_writer.WriteText(base::UTF8ToUTF16(extracted_text.url.spec()));
    }
  }

  const bool has_files =
      (intent->files.has_value() && !intent->files.value().empty());
  if (has_files) {
    std::vector<ui::FileInfo> file_infos;
    for (const auto& file : intent->files.value()) {
      file_infos.emplace_back(
          ui::FileInfo(apps::GetFileSystemURL(profile_, file->url).path(),
                       base::FilePath()));
    }

    clipboard_writer.WriteFilenames(ui::FileInfosToURIList(file_infos));
  }

  // TODO(crbug.com/1244143) Add image copying logic.
  if (controller_) {
    controller_->CloseBubble(::sharesheet::SharesheetResult::kSuccess);
  }
}

void CopyToClipboardShareAction::OnClosing(
    ::sharesheet::SharesheetController* controller) {
  controller_ = nullptr;
}

}  // namespace sharesheet
}  // namespace ash
