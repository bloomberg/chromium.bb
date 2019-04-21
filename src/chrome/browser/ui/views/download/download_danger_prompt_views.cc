// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/compiler_specific.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

using safe_browsing::ClientSafeBrowsingReportRequest;

namespace {

// Views-specific implementation of download danger prompt dialog. We use this
// class rather than a TabModalConfirmDialog so that we can use custom
// formatting on the text in the body of the dialog.
class DownloadDangerPromptViews : public DownloadDangerPrompt,
                                  public download::DownloadItem::Observer,
                                  public views::DialogDelegateView {
 public:
  DownloadDangerPromptViews(download::DownloadItem* item,
                            Profile* profile,
                            bool show_context,
                            const OnDone& done);
  ~DownloadDangerPromptViews() override;

  // DownloadDangerPrompt:
  void InvokeActionForTesting(Action action) override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  base::string16 GetAcceptButtonTitle() const;
  base::string16 GetCancelButtonTitle() const;
  base::string16 GetMessageBody() const;
  void RunDone(Action action);

  download::DownloadItem* download_;
  Profile* profile_;
  // If show_context_ is true, this is a download confirmation dialog by
  // download API, otherwise it is download recovery dialog from a regular
  // download.
  bool show_context_;
  OnDone done_;
};

DownloadDangerPromptViews::DownloadDangerPromptViews(
    download::DownloadItem* item,
    Profile* profile,
    bool show_context,
    const OnDone& done)
    : download_(item),
      profile_(profile),
      show_context_(show_context),
      done_(done) {
  download_->AddObserver(this);

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::Label* message_body_label = new views::Label(GetMessageBody());
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetAllowCharacterBreak(true);

  AddChildView(message_body_label);

  RecordOpenedDangerousConfirmDialog(download_->GetDangerType());

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::DOWNLOAD_DANGER_PROMPT);
}

DownloadDangerPromptViews::~DownloadDangerPromptViews() {
  if (download_)
    download_->RemoveObserver(this);
}

// DownloadDangerPrompt methods:
void DownloadDangerPromptViews::InvokeActionForTesting(Action action) {
  switch (action) {
    case ACCEPT:
      // This inversion is intentional.
      Cancel();
      break;

    case DISMISS:
      Close();
      break;

    case CANCEL:
      Accept();
      break;

    default:
      NOTREACHED();
      break;
  }
}

// views::DialogDelegate methods:
base::string16 DownloadDangerPromptViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return GetAcceptButtonTitle();

    case ui::DIALOG_BUTTON_CANCEL:
      return GetCancelButtonTitle();

    default:
      return DialogDelegate::GetDialogButtonLabel(button);
  }
}

base::string16 DownloadDangerPromptViews::GetWindowTitle() const {
  if (show_context_ || !download_)  // |download_| may be null in tests.
    return l10n_util::GetStringUTF16(IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
  switch (download_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return l10n_util::GetStringUTF16(IDS_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return l10n_util::GetStringUTF16(IDS_KEEP_UNCOMMON_DOWNLOAD_TITLE);
    default: {
      return l10n_util::GetStringUTF16(
          IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    }
  }
}

ui::ModalType DownloadDangerPromptViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool DownloadDangerPromptViews::Accept() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Note that the presentational concept of "Accept/Cancel" is inverted from
  // the model's concept of ACCEPT/CANCEL. In the UI, the safe path is "Accept"
  // and the dangerous path is "Cancel".
  RunDone(CANCEL);
  return true;
}

bool DownloadDangerPromptViews::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RunDone(ACCEPT);
  return true;
}

bool DownloadDangerPromptViews::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RunDone(DISMISS);
  return true;
}

// download::DownloadItem::Observer:
void DownloadDangerPromptViews::OnDownloadUpdated(
    download::DownloadItem* download) {
  // If the download is nolonger dangerous (accepted externally) or the download
  // is in a terminal state, then the download danger prompt is no longer
  // necessary.
  if (!download_->IsDangerous() || download_->IsDone()) {
    RunDone(DISMISS);
    Cancel();
  }
}

gfx::Size DownloadDangerPromptViews::CalculatePreferredSize() const {
  int preferred_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                        margins().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

base::string16 DownloadDangerPromptViews::GetAcceptButtonTitle() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

base::string16 DownloadDangerPromptViews::GetCancelButtonTitle() const {
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD);
  return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN);
}

base::string16 DownloadDangerPromptViews::GetMessageBody() const {
  if (show_context_) {
    switch (download_->GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_DANGEROUS_DOWNLOAD,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:  // Fall through
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
        if (safe_browsing::AdvancedProtectionStatusManager::
                RequestsAdvancedProtectionVerdicts(profile_)) {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT_IN_ADVANCED_PROTECTION,
              download_->GetFileNameToReportUser().LossyDisplayName());
        } else {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
              download_->GetFileNameToReportUser().LossyDisplayName());
        }
      }
      case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
        if (safe_browsing::AdvancedProtectionStatusManager::
                RequestsAdvancedProtectionVerdicts(profile_)) {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT_IN_ADVANCED_PROTECTION,
              download_->GetFileNameToReportUser().LossyDisplayName());
        } else {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
              download_->GetFileNameToReportUser().LossyDisplayName());
        }
      }
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
        if (safe_browsing::AdvancedProtectionStatusManager::
                RequestsAdvancedProtectionVerdicts(profile_)) {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS_IN_ADVANCED_PROTECTION,
              download_->GetFileNameToReportUser().LossyDisplayName());
        } else {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
              download_->GetFileNameToReportUser().LossyDisplayName());
        }
      }
      case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      case download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY:
      case download::DOWNLOAD_DANGER_TYPE_MAX: {
        break;
      }
    }
  } else {
    switch (download_->GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_CONFIRM_KEEP_MALICIOUS_DOWNLOAD_BODY);
      }
      default: {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_CONFIRM_KEEP_DANGEROUS_DOWNLOAD);
      }
    }
  }
  NOTREACHED();
  return base::string16();
}

void DownloadDangerPromptViews::RunDone(Action action) {
  // Invoking the callback can cause the download item state to change or cause
  // the window to close, and |callback| refers to a member variable.
  OnDone done = done_;
  done_.Reset();
  if (download_ != NULL) {
    // If this download is no longer dangerous, is already canceled or
    // completed, don't send any report.
    if (download_->IsDangerous() && !download_->IsDone()) {
      const bool accept = action == DownloadDangerPrompt::ACCEPT;
      RecordDownloadDangerPrompt(accept, *download_);
      if (!download_->GetURL().is_empty() &&
          !content::DownloadItemUtils::GetBrowserContext(download_)
               ->IsOffTheRecord()) {
        ClientSafeBrowsingReportRequest::ReportType report_type
            = show_context_ ?
                ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API :
                ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY;
        SendSafeBrowsingDownloadReport(report_type, accept, *download_);
      }
    }
    download_->RemoveObserver(this);
    download_ = NULL;
  }
  if (!done.is_null())
    done.Run(action);
}

}  // namespace

// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    download::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DownloadDangerPromptViews* download_danger_prompt =
      new DownloadDangerPromptViews(item, profile, show_context, done);
  constrained_window::ShowWebModalDialogViews(download_danger_prompt,
                                              web_contents);
  return download_danger_prompt;
}
