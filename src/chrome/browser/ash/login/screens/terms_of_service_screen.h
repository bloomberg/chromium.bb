// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"

namespace network {
class SimpleURLLoader;
}

namespace ash {

// A screen that shows Terms of Service which have been configured through
// policy. The screen is shown during login and requires the user to accept the
// Terms of Service before proceeding. Try to load online version of Terms of
// Service, if it isn't available due to any reason - load locally saved terms
// from file. If there is no locally saved terms - show an error message.
class TermsOfServiceScreen : public BaseScreen {
 public:
  enum class Result { ACCEPTED, DECLINED, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  // The possible states that the screen may assume.
  enum class ScreenState : int { LOADING = 0, LOADED = 1, ERROR = 2 };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  TermsOfServiceScreen(TermsOfServiceScreenView* view,
                       const ScreenExitCallback& exit_callback);

  TermsOfServiceScreen(const TermsOfServiceScreen&) = delete;
  TermsOfServiceScreen& operator=(const TermsOfServiceScreen&) = delete;

  ~TermsOfServiceScreen() override;

  // Called when the user declines the Terms of Service.
  void OnDecline();

  // Called when the user accepts the Terms of Service.
  void OnAccept();

  // Called when the user retries to obtain the Terms of Service.
  void OnRetry();

  // Called when view is destroyed so there is no dead reference to it.
  void OnViewDestroyed(TermsOfServiceScreenView* view);

  // Set callback to wait for file saving in tests.
  static void SetTosSavedCallbackForTesting(base::OnceClosure callback);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  // Get path for a user terms of service file.
  static base::FilePath GetTosFilePath();
 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  // Start downloading the Terms of Service.
  void StartDownload();

  // Abort the attempt to download the Terms of Service if it takes too long.
  void OnDownloadTimeout();

  // Callback function called when SimpleURLLoader completes.
  void OnDownloaded(std::unique_ptr<std::string> response_body);

  // Try to load terms of service from file, show error if there is a failure.
  void LoadFromFileOrShowError();
  // Show terms of service once they are loaded from file.
  void OnTosLoadedFromFile(absl::optional<std::string> tos);
  // Save terms as text to a local file.
  void SaveTos(const std::string& tos);
  // Runs callback for tests.
  void OnTosSavedForTesting();

  TermsOfServiceScreenView* view_;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<network::SimpleURLLoader> terms_of_service_loader_;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the Terms of Service.
  base::OneShotTimer download_timer_;

  base::WeakPtrFactory<TermsOfServiceScreen> weak_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::TermsOfServiceScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_
