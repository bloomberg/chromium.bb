// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aura {
class Window;
}

namespace ash {

// InSessionAuthDialogController manages the in-session auth dialog.
class ASH_PUBLIC_EXPORT InSessionAuthDialogController {
 public:
  // Callback for authentication checks. |success| is nullopt if an
  // authentication check did not run, otherwise it is true/false if auth
  // succeeded/failed.
  using OnAuthenticateCallback =
      base::OnceCallback<void(absl::optional<bool> success)>;
  // Callback for overall authentication flow result.
  using FinishCallback = base::OnceCallback<void(bool success)>;

  // Return the singleton instance.
  static InSessionAuthDialogController* Get();

  // Sets the client that will handle authentication.
  virtual void SetClient(InSessionAuthDialogClient* client) = 0;

  // Displays the authentication dialog for the website/app name in |app_id|.
  virtual void ShowAuthenticationDialog(aura::Window* source_window,
                                        const std::string& origin_name,
                                        FinishCallback finish_callback) = 0;

  // Destroys the authentication dialog.
  virtual void DestroyAuthenticationDialog() = 0;

  // Takes a PIN and sends it to InSessionAuthDialogClient to
  // authenticate. The InSessionAuthDialogClient should already know the current
  // session's active user, so the user account is not provided here.
  virtual void AuthenticateUserWithPin(const std::string& pin,
                                       OnAuthenticateCallback callback) = 0;

  // Requests ChromeOS to report fingerprint scan result through |callback|.
  virtual void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, FingerprintState)> callback) = 0;

  // Opens a help article in Chrome.
  virtual void OpenInSessionAuthHelpPage() = 0;

  // Cancels all operations and destroys the dialog.
  virtual void Cancel() = 0;

  // Checks whether there's at least one authentication method.
  virtual void CheckAvailability(
      FinishCallback on_availability_checked) const = 0;

 protected:
  InSessionAuthDialogController();
  virtual ~InSessionAuthDialogController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
