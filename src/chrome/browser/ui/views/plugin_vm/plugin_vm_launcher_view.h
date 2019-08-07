// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_LAUNCHER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_LAUNCHER_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class ImageView;
class Label;
class ProgressBar;
}  // namespace views

class Profile;

// The PluginVm launcher that is shown to user if PluginVm icon is clicked, but
// PluginVm is not yet ready to be launched. This class is responsible for
// triggering the steps of the PluginVm setup process and displaying progress
// according to the state of this setup.
class PluginVmLauncherView : public views::BubbleDialogDelegateView,
                             public plugin_vm::PluginVmImageManager::Observer {
 public:
  explicit PluginVmLauncherView(Profile* profile);

  // views::BubbleDialogDelegateView implementation.
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

  // plugin_vm::PluginVmImageDownloadObserver implementation.
  void OnDownloadStarted() override;
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length,
                                 int64_t download_bytes_per_sec) override;
  void OnDownloadCompleted() override;
  void OnDownloadCancelled() override;
  void OnDownloadFailed() override;
  void OnImportProgressUpdated(uint64_t percent_completed,
                               int64_t import_percent_per_sec) override;
  void OnImported() override;
  void OnImportCancelled() override;
  void OnImportFailed() override;

  // Public for testing purposes.
  base::string16 GetBigMessage() const;
  base::string16 GetMessage() const;

 protected:
  enum class State {
    START_DOWNLOADING,  // PluginVm image downloading should be started.
    DOWNLOADING,        // PluginVm image downloading is in progress.
    IMPORTING,          // Downloaded PluginVm image importing is in progress.
    FINISHED,           // PluginVm environment setting has been finished.
    ERROR,              // Something unexpected happened.
    NOT_ALLOWED,        // PluginVm is disallowed on the device.
  };

  State state_ = State::START_DOWNLOADING;

  ~PluginVmLauncherView() override;
  virtual void OnStateUpdated();
  // views::BubbleDialogDelegateView implementation.
  void AddedToWidget() override;

 private:
  base::string16 GetDownloadProgressMessage(uint64_t downlaoded_bytes,
                                            int64_t content_length) const;
  // Returns empty string in case time left cannot be estimated.
  base::string16 GetTimeLeftMessage(int64_t processed_bytes,
                                    int64_t bytes_to_be_processed,
                                    int64_t bytes_per_sec) const;
  void SetBigMessageLabel();
  void SetMessageLabel();
  void SetBigImage();

  void StartPluginVmImageDownload();

  Profile* profile_ = nullptr;
  plugin_vm::PluginVmImageManager* plugin_vm_image_manager_ = nullptr;
  views::Label* big_message_label_ = nullptr;
  views::Label* message_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;
  views::Label* download_progress_message_label_ = nullptr;
  views::Label* time_left_message_label_ = nullptr;
  views::ImageView* big_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PluginVmLauncherView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_LAUNCHER_VIEW_H_
