// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_

#include <memory>

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_view.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Android implementation of the modal prompt for saving new/updating existing
// address profile. The class is responsible for showing the view and handling
// user interactions. The controller owns its java counterpart and the
// corresponding view.
class SaveUpdateAddressProfilePromptController {
 public:
  SaveUpdateAddressProfilePromptController(
      std::unique_ptr<SaveUpdateAddressProfilePromptView> prompt_view,
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::AddressProfileSavePromptCallback decision_callback,
      base::OnceCallback<void()> dismissal_callback);
  SaveUpdateAddressProfilePromptController(
      const SaveUpdateAddressProfilePromptController&) = delete;
  SaveUpdateAddressProfilePromptController& operator=(
      const SaveUpdateAddressProfilePromptController&) = delete;
  ~SaveUpdateAddressProfilePromptController();

  void DisplayPrompt();

  std::u16string GetTitle();
  std::u16string GetPositiveButtonText();
  std::u16string GetNegativeButtonText();
  // For save prompt:
  std::u16string GetAddress();
  std::u16string GetEmail();
  std::u16string GetPhoneNumber();
  // For update prompt:
  std::u16string GetSubtitle();
  // Returns two strings listing formatted profile data that will change when
  // the `original_profile_` is updated to `profile_`. The old values, which
  // will be replaced, are the first value, and the new values, which will be
  // saved, are the second value.
  std::pair<std::u16string, std::u16string> GetDiffFromOldToNewProfile();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void OnUserAccepted(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnUserDeclined(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnUserEdited(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jobject>& jprofile);
  // Called whenever the prompt is dismissed (e.g. because the user already
  // accepted/declined/edited the profile (after OnUserAccepted/Declined/Edited
  // is called) or it was closed without interaction).
  void OnPromptDismissed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

 private:
  void RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision decision);

  // If the user explicitly accepted/dismissed/edited the profile.
  bool had_user_interaction_ = false;
  // View that displays the prompt.
  std::unique_ptr<SaveUpdateAddressProfilePromptView> prompt_view_;
  // The profile which is being confirmed by the user.
  AutofillProfile profile_;
  // The profile (if exists) which will be updated if the user confirms.
  absl::optional<AutofillProfile> original_profile_;
  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback decision_callback_;
  // The callback guaranteed to be run once the prompt is dismissed.
  base::OnceCallback<void()> dismissal_callback_;
  // The corresponding Java SaveUpdateAddressProfilePromptController.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_
