// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sharesheet/copy_to_clipboard_share_action.h"

#include <string>

#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_split.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {
const char kToastId[] = "copy_to_clipboard_share_action";
const int kToastDurationMs = 2500;
const char kMimeTypeSeparator[] = "/";

::sharesheet::SharesheetMetrics::MimeType ConvertMimeTypeForMetrics(
    std::string mime_type) {
  std::vector<std::string> type =
      base::SplitString(mime_type, kMimeTypeSeparator, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (type.size() == 0) {
    return ::sharesheet::SharesheetMetrics::MimeType::kUnknown;
  }

  if (type[0] == "text") {
    return ::sharesheet::SharesheetMetrics::MimeType::kTextFile;
  } else if (type[0] == "image") {
    return ::sharesheet::SharesheetMetrics::MimeType::kImageFile;
  } else if (type[0] == "video") {
    return ::sharesheet::SharesheetMetrics::MimeType::kVideoFile;
  } else if (type[0] == "audio") {
    return ::sharesheet::SharesheetMetrics::MimeType::kAudioFile;
  } else if (mime_type == "application/pdf") {
    return ::sharesheet::SharesheetMetrics::MimeType::kPdfFile;
  } else {
    return ::sharesheet::SharesheetMetrics::MimeType::kUnknown;
  }
}

void RecordMimeTypes(
    const base::flat_set<::sharesheet::SharesheetMetrics::MimeType>&
        mime_types_to_record) {
  for (auto& mime_type : mime_types_to_record) {
    ::sharesheet::SharesheetMetrics::RecordCopyToClipboardShareActionMimeType(
        mime_type);
  }
}

}  // namespace

namespace ash {
namespace sharesheet {

using ::sharesheet::SharesheetMetrics;

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
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  base::flat_set<SharesheetMetrics::MimeType> mime_types_to_record;

  if (intent->share_text.has_value()) {
    apps_util::SharedText extracted_text =
        apps_util::ExtractSharedText(intent->share_text.value());

    if (!extracted_text.text.empty()) {
      clipboard_writer.WriteText(base::UTF8ToUTF16(extracted_text.text));
      mime_types_to_record.insert(SharesheetMetrics::MimeType::kText);
    }

    if (!extracted_text.url.is_empty()) {
      std::string anchor_text;
      if (intent->share_title.has_value() &&
          !(intent->share_title.value().empty())) {
        anchor_text = intent->share_title.value();
      }
      clipboard_writer.WriteText(base::UTF8ToUTF16(extracted_text.url.spec()));
      mime_types_to_record.insert(SharesheetMetrics::MimeType::kUrl);
    }
  }

  const bool has_files =
      (intent->files.has_value() && !intent->files.value().empty());
  if (has_files) {
    std::vector<ui::FileInfo> file_infos;
    for (const auto& file : intent->files.value()) {
      auto file_url = apps::GetFileSystemURL(profile_, file->url);
      // TODO(crbug.com/1274983) : Add support for copying from MTP and
      // FileSystemProviders.
      if (!file_manager::util::IsNonNativeFileSystemType(file_url.type())) {
        file_infos.emplace_back(
            ui::FileInfo(file_url.path(), base::FilePath()));
        if (file->mime_type.has_value())
          mime_types_to_record.insert(
              ConvertMimeTypeForMetrics(file->mime_type.value()));
      }
    }
    clipboard_writer.WriteFilenames(ui::FileInfosToURIList(file_infos));
  }

  RecordMimeTypes(mime_types_to_record);

  if (controller) {
    controller->CloseBubble(::sharesheet::SharesheetResult::kSuccess);
  }

  ToastData toast(kToastId,
                  l10n_util::GetStringUTF16(
                      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SUCCESS_TOAST_LABEL),
                  kToastDurationMs,
                  /*dismiss_text=*/absl::nullopt,
                  /*visible_on_lock_screen=*/false);
  ShowToast(toast);
}

void CopyToClipboardShareAction::OnClosing(
    ::sharesheet::SharesheetController* controller) {}

bool CopyToClipboardShareAction::ShouldShowAction(
    const apps::mojom::IntentPtr& intent,
    bool contains_hosted_document) {
  bool contains_uncopyable_file = false;
  const bool has_files =
      (intent->files.has_value() && !intent->files.value().empty());
  if (has_files) {
    for (const auto& file : intent->files.value()) {
      auto file_url = apps::GetFileSystemURL(profile_, file->url);
      contains_uncopyable_file =
          file_manager::util::IsNonNativeFileSystemType(file_url.type());
      if (contains_uncopyable_file) {
        break;
      }
    }
  }
  // If |intent| contains a file we can't copy, don't show this action.
  // Files that are not in a native local file system (e.g. MTP, documents
  // providers) do not currently support paste outside of the Files app.
  return !contains_uncopyable_file &&
         ShareAction::ShouldShowAction(intent, contains_hosted_document);
}

void CopyToClipboardShareAction::ShowToast(const ash::ToastData& toast_data) {
  ToastManager::Get()->Show(toast_data);
}

}  // namespace sharesheet
}  // namespace ash
