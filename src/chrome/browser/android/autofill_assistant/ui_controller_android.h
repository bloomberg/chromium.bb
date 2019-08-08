// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/autofill_assistant/assistant_form_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_header_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_overlay_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_payment_request_delegate.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/overlay_state.h"
#include "components/autofill_assistant/browser/ui_controller.h"

namespace autofill_assistant {
// Class implements UiController, Client and starts the Controller.
// TODO(crbug.com/806868): This class should be renamed to
// AssistantMediator(Android) and listen for state changes to forward those
// changes to the UI model.
class UiControllerAndroid : public UiController {
 public:
  static std::unique_ptr<UiControllerAndroid> CreateFromWebContents(
      content::WebContents* web_contents);

  // pointers to |web_contents|, |client| must remain valid for the lifetime of
  // this instance.
  //
  // Pointer to |ui_delegate| must remain valid for the lifetime of this
  // instance or until WillShutdown is called.
  UiControllerAndroid(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jactivity);
  ~UiControllerAndroid() override;

  // Attaches the UI to the given client, its web contents and delegate.
  //
  // |web_contents| and |client| must remain valid for the lifetime of this
  // instance or until Attach() is called again, with different pointers.
  //
  // |ui_delegate| must remain valid for the lifetime of this instance or until
  // either Attach() or WillShutdown() are called.
  void Attach(content::WebContents* web_contents,
              Client* client,
              UiDelegate* ui_delegate);

  // Called by ClientAndroid.
  void ShowOnboarding(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jexperiment_ids,
      const base::android::JavaParamRef<jobject>& on_accept);

  // Overrides UiController:
  void OnStateChanged(AutofillAssistantState new_state) override;
  void OnStatusMessageChanged(const std::string& message) override;
  void WillShutdown(Metrics::DropOutReason reason) override;
  void OnSuggestionsChanged(const std::vector<Chip>& suggestions) override;
  void OnActionsChanged(const std::vector<Chip>& actions) override;
  void OnPaymentRequestOptionsChanged(
      const PaymentRequestOptions* options) override;
  void OnPaymentRequestInformationChanged(
      const PaymentInformation* state) override;
  void OnDetailsChanged(const Details* details) override;
  void OnInfoBoxChanged(const InfoBox* info_box) override;
  void OnProgressChanged(int progress) override;
  void OnProgressVisibilityChanged(bool visible) override;
  void OnTouchableAreaChanged(const RectF& visual_viewport,
                              const std::vector<RectF>& areas) override;
  void OnResizeViewportChanged(bool resize_viewport) override;
  void OnPeekModeChanged(
      ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void OnOverlayColorsChanged(const UiDelegate::OverlayColors& colors) override;
  void OnFormChanged(const FormProto* form) override;

  // Called by AssistantOverlayDelegate:
  void OnUnexpectedTaps();
  void UpdateTouchableArea();
  void OnUserInteractionInsideTouchableArea();

  // Called by AssistantHeaderDelegate:
  void OnFeedbackButtonClicked();

  // Called by AssistantPaymentRequestDelegate:
  void OnShippingAddressChanged(
      std::unique_ptr<autofill::AutofillProfile> address);
  void OnBillingAddressChanged(
      std::unique_ptr<autofill::AutofillProfile> address);
  void OnContactInfoChanged(std::string name,
                            std::string phone,
                            std::string email);
  void OnCreditCardChanged(std::unique_ptr<autofill::CreditCard> card);
  void OnTermsAndConditionsChanged(TermsAndConditionsState state);

  // Called by AssistantFormDelegate:
  void OnCounterChanged(int input_index, int counter_index, int value);
  void OnChoiceSelectionChanged(int input_index,
                                int choice_index,
                                bool selected);

  // Called by Java.
  void SnackbarResult(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jboolean undo);
  void Stop(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            int reason);
  void OnFatalError(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& message,
                    int reason);
  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnSuggestionSelected(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            jint index);
  void OnActionSelected(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcaller,
                        jint index);
  void OnCancelButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint actionIndex);
  void OnCloseButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void SetVisible(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller,
                  jboolean visible);

 private:
  // A pointer to the client. nullptr until Attach() is called.
  Client* client_ = nullptr;

  // A pointer to the ui_delegate. nullptr until Attach() is called.
  UiDelegate* ui_delegate_ = nullptr;
  AssistantOverlayDelegate overlay_delegate_;
  AssistantHeaderDelegate header_delegate_;
  AssistantPaymentRequestDelegate payment_request_delegate_;
  AssistantFormDelegate form_delegate_;

  // What to do if undo is not pressed on the current snackbar.
  base::OnceCallback<void()> snackbar_action_;

  base::android::ScopedJavaLocalRef<jobject> GetModel();
  base::android::ScopedJavaLocalRef<jobject> GetOverlayModel();
  base::android::ScopedJavaLocalRef<jobject> GetHeaderModel();
  base::android::ScopedJavaLocalRef<jobject> GetDetailsModel();
  base::android::ScopedJavaLocalRef<jobject> GetInfoBoxModel();
  base::android::ScopedJavaLocalRef<jobject> GetPaymentRequestModel();
  base::android::ScopedJavaLocalRef<jobject> GetFormModel();

  void SetOverlayState(OverlayState state);
  void AllowShowingSoftKeyboard(bool enabled);
  void ExpandBottomSheet();
  void SetSpinPoodle(bool enabled);
  std::string GetDebugContext();
  void DestroySelf();
  void Shutdown(Metrics::DropOutReason reason);
  void UpdateActions();

  // Hide the UI, show a snackbar with an undo button, and execute the given
  // action after a short delay unless the user taps the undo button.
  void ShowSnackbar(const std::string& message,
                    base::OnceCallback<void()> action);
  void OnCancelButtonClicked();
  void OnCancelButtonWithActionIndexClicked(int action_index);
  void OnCancel(int action_index);

  // Updates the state of the UI to reflect the UIDelegate's state.
  void SetupForState();

  // Makes the whole of AA invisible or visible again.
  void SetVisible(bool visible);

  // Debug context captured previously. If non-empty, GetDebugContext() returns
  // this context.
  std::string captured_debug_context_;

  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  OverlayState desired_overlay_state_ = OverlayState::FULL;
  base::WeakPtrFactory<UiControllerAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
