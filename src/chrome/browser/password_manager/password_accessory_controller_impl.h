// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "components/autofill/core/browser/accessory_sheet_data.h"
#include "components/autofill/core/common/filling_status.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

class ManualFillingController;
class PasswordGenerationDialogViewInterface;

// Use either PasswordAccessoryController::GetOrCreate or
// PasswordAccessoryController::GetIfExisting to obtain instances of this class.
//
// TODO(crbug.com/854150): This class currently only supports credentials
// originating from the main frame. Supporting iframes is intended.
class PasswordAccessoryControllerImpl
    : public PasswordAccessoryController,
      public content::WebContentsUserData<PasswordAccessoryControllerImpl> {
 public:
  using CreateDialogFactory = base::RepeatingCallback<std::unique_ptr<
      PasswordGenerationDialogViewInterface>(PasswordAccessoryController*)>;

  ~PasswordAccessoryControllerImpl() override;

  // PasswordAccessoryController:
  void SavePasswordsForOrigin(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const url::Origin& origin) override;
  void OnAutomaticGenerationStatusChanged(
      bool available,
      const base::Optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver)
      override;
  void OnFilledIntoFocusedField(autofill::FillingStatus status) override;
  void RefreshSuggestionsForField(const url::Origin& origin,
                                  bool is_fillable,
                                  bool is_password_field) override;
  void DidNavigateMainFrame() override;
  void ShowWhenKeyboardIsVisible() override;
  void Hide() override;
  void GetFavicon(
      int desired_size_in_pixel,
      base::OnceCallback<void(const gfx::Image&)> icon_callback) override;
  void OnFillingTriggered(bool is_password,
                          const base::string16& text_to_fill) override;
  void OnOptionSelected(const base::string16& selected_option) const override;
  void OnGenerationRequested() override;
  void GeneratedPasswordAccepted(const base::string16& password) override;
  void GeneratedPasswordRejected() override;
  gfx::NativeWindow native_window() const override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a manual filling
  // controller.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      CreateDialogFactory create_dialog_callback,
      favicon::FaviconService* favicon_service);

 private:
  // Data including the form and field for which generation was requested,
  // their signatures and the maximum password size.
  struct GenerationElementData;

  // Data for a credential pair that is transformed into a suggestion.
  struct SuggestionElementData;

  // Data allowing to cache favicons and favicon-related requests.
  struct FaviconRequestData;

  friend class content::WebContentsUserData<PasswordAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit PasswordAccessoryControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock or fake view.
  PasswordAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      CreateDialogFactory create_dialog_callback,
      favicon::FaviconService* favicon_service);

  // Creates the view items based on the given |suggestions|.
  // If |is_password_field| is false, password suggestions won't be interactive.
  static autofill::AccessorySheetData CreateAccessorySheetData(
      const url::Origin& origin,
      const std::vector<SuggestionElementData>& suggestions,
      bool is_password_field);

  // Handles a favicon response requested by |GetFavicon| and responds to
  // pending favicon requests with a (possibly empty) icon bitmap.
  void OnImageFetched(
      url::Origin origin,
      const favicon_base::FaviconRawBitmapResult& bitmap_results);

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // Contains the last set of credentials by origin.
  std::map<url::Origin, std::vector<SuggestionElementData>> origin_suggestions_;

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // Data for the generation element used to generate the password.
  std::unique_ptr<GenerationElementData> generation_element_data_;

  // The origin of the currently focused frame. It's used to ensure that
  // favicons are not displayed across origins.
  url::Origin current_origin_;

  // TODO(fhorschig): Find a way to use unordered_map with origin keys.
  // A cache for all favicons that were requested. This includes all iframes
  // for which the accessory was displayed.
  std::map<url::Origin, FaviconRequestData> icons_request_data_;

  // Used to track a requested favicon. If the set of suggestion changes, this
  // object aborts the request. Upon destruction, requests are cancelled, too.
  base::CancelableTaskTracker favicon_tracker_;

  // Password manager driver for the target frame used for password generation.
  base::WeakPtr<password_manager::PasswordManagerDriver> target_frame_driver_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // Modal dialog view meant to display the generated password.
  std::unique_ptr<PasswordGenerationDialogViewInterface> dialog_view_;

  // Remembers whether the last focused field was a password field. That way,
  // the reconstructed elements have the correct type.
  bool last_focused_field_was_for_passwords_ = false;

  // Creation callback for the modal dialog view meant to facilitate testing.
  CreateDialogFactory create_dialog_factory_;

  // The favicon service used to make retrieve icons for a given origin.
  favicon::FaviconService* favicon_service_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryControllerImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
