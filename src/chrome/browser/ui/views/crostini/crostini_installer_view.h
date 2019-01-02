// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_INSTALLER_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class ImageView;
class Label;
class Link;
class ProgressBar;
}  // namespace views

namespace crostini {
enum class ConciergeClientResult;
}  // namespace crostini

class Profile;

// The Crostini installer. Provides details about Crostini to the user and
// installs it if the user chooses to do so.
class CrostiniInstallerView
    : public views::DialogDelegateView,
      public views::LinkListener,
      public crostini::CrostiniManager::RestartObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SetupResult {
    kNotStarted = 0,
    kUserCancelled = 1,
    kSuccess = 2,
    kErrorLoadingTermina = 3,
    kErrorStartingConcierge = 4,
    kErrorCreatingDiskImage = 5,
    kErrorStartingTermina = 6,
    kErrorStartingContainer = 7,
    kErrorOffline = 8,
    kErrorFetchingSshKeys = 9,
    kErrorMountingContainer = 10,
    kCount
  };

  static void Show(Profile* profile);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // crostini::CrostiniManager::RestartObserver
  void OnComponentLoaded(crostini::ConciergeClientResult result) override;
  void OnConciergeStarted(crostini::ConciergeClientResult result) override;
  void OnDiskImageCreated(crostini::ConciergeClientResult result) override;
  void OnVmStarted(crostini::ConciergeClientResult result) override;
  void OnContainerStarted(crostini::ConciergeClientResult result) override;
  void OnSshKeysFetched(crostini::ConciergeClientResult result) override;

  static CrostiniInstallerView* GetActiveViewForTesting();

 private:
  enum class State {
    PROMPT,  // Prompting the user to allow installation.
    ERROR,   // Something unexpected happened.
    // We automatically progress through the following steps.
    INSTALL_START,         // The user has just clicked 'Install'.
    INSTALL_IMAGE_LOADER,  // Loading the Termina VM component.
    START_CONCIERGE,       // Starting the Concierge D-Bus client.
    CREATE_DISK_IMAGE,     // Creating the image for the Termina VM.
    START_TERMINA_VM,      // Starting the Termina VM.
    START_CONTAINER,       // Starting the container inside the Termina VM.
    FETCH_SSH_KEYS,        // Fetch ssh keys from concierge.
    MOUNT_CONTAINER,       // Do sshfs mount of container.
    SHOW_LOGIN_SHELL,      // Showing a new crosh window.
    INSTALL_END = SHOW_LOGIN_SHELL,  // Marker enum for last install state.
  };

  explicit CrostiniInstallerView(Profile* profile);
  ~CrostiniInstallerView() override;

  void HandleError(const base::string16& error_message, SetupResult result);
  void MountContainerFinished(crostini::ConciergeClientResult result);
  void ShowLoginShell();
  void StepProgress();
  void SetMessageLabel();
  void SetBigMessageLabel();

  void RecordSetupResultHistogram(SetupResult result);

  State state_ = State::PROMPT;
  views::ImageView* logo_image_ = nullptr;
  views::Label* big_message_label_ = nullptr;
  views::Label* message_label_ = nullptr;
  views::Link* learn_more_link_ = nullptr;
  views::ImageView* big_image_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;
  Profile* profile_;
  crostini::CrostiniManager::RestartId restart_id_ =
      crostini::CrostiniManager::kUninitializedRestartId;

  // Whether the result has been logged or not is stored to prevent multiple
  // results being logged for a given setup flow. This can happen due to
  // multiple error callbacks happening in some cases, as well as the user being
  // able to hit Cancel after any errors occur.
  bool has_logged_result_ = false;

  base::WeakPtrFactory<CrostiniInstallerView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
