// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
class EventHandler;
class UserModel;

class MockActionDelegate : public ActionDelegate {
 public:
  MockActionDelegate();
  ~MockActionDelegate() override;

  MOCK_METHOD1(RunElementChecks, void(BatchElementChecker*));

  void ShortWaitForElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnShortWaitForElement(selector, callback);
  }

  MOCK_METHOD2(OnShortWaitForElement,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>&));

  void WaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnWaitForDom(max_wait_time, allow_interrupt, check_elements, callback);
  }

  MOCK_METHOD4(OnWaitForDom,
               void(base::TimeDelta,
                    bool,
                    base::RepeatingCallback<
                        void(BatchElementChecker*,
                             base::OnceCallback<void(const ClientStatus&)>)>&,
                    base::OnceCallback<void(const ClientStatus&)>&));

  MOCK_METHOD1(SetStatusMessage, void(const std::string& message));
  MOCK_METHOD0(GetStatusMessage, std::string());
  MOCK_METHOD1(SetBubbleMessage, void(const std::string& message));
  MOCK_METHOD0(GetBubbleMessage, std::string());
  MOCK_METHOD3(ClickOrTapElement,
               void(const Selector& selector,
                    ClickType click_type,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD4(Prompt,
               void(std::unique_ptr<std::vector<UserAction>> user_actions,
                    bool disable_force_expand_sheet,
                    base::OnceCallback<void()> end_on_navigation_callback,
                    bool browse_mode));
  MOCK_METHOD0(CleanUpAfterPrompt, void());
  MOCK_METHOD1(SetBrowseDomainsWhitelist,
               void(std::vector<std::string> domains));

  void FillAddressForm(
      const autofill::AutofillProfile* profile,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFillAddressForm(profile, selector, callback);
  }

  MOCK_METHOD3(OnFillAddressForm,
               void(const autofill::AutofillProfile* profile,
                    const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const base::string16& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFillCardForm(card.get(), cvc, selector, callback);
  }

  void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback)
      override {
    autofill::FormData form_data;
    autofill::FormFieldData field_data;
    std::move(callback).Run(OkClientStatus(), form_data, field_data);
  }

  MOCK_METHOD4(OnFillCardForm,
               void(const autofill::CreditCard* card,
                    const base::string16& cvc,
                    const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD4(SelectOption,
               void(const Selector& selector,
                    const std::string& value,
                    DropdownSelectStrategy select_strategy,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(FocusElement,
               void(const Selector& selector,
                    const TopPadding& top_padding,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD1(SetTouchableElementArea,
               void(const ElementAreaProto& touchable_element_area));

  MOCK_METHOD2(HighlightElement,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD1(CollectUserData,
               void(CollectUserDataOptions* collect_user_data_options));
  MOCK_METHOD1(
      WriteUserData,
      void(base::OnceCallback<void(UserData*, UserData::FieldChange*)>));
  MOCK_METHOD1(WriteUserModel, void(base::OnceCallback<void(UserModel*)>));

  MOCK_METHOD1(OnGetFullCard,
               void(base::OnceCallback<void(const autofill::CreditCard& card,
                                            const base::string16& cvc)>&));

  void GetFullCard(GetFullCardCallback callback) override {
    // A local variable is necessary to allow OnGetFullCard to get a reference.
    base::OnceCallback<void(const autofill::CreditCard& card,
                            const base::string16& cvc)>
        transformed_callback = base::BindOnce(
            [](GetFullCardCallback real_callback,
               const autofill::CreditCard& card, const base::string16& cvc) {
              std::move(real_callback)
                  .Run(std::make_unique<autofill::CreditCard>(card), cvc);
            },
            std::move(callback));
    OnGetFullCard(transformed_callback);
  }

  void GetFieldValue(const Selector& selector,
                     base::OnceCallback<void(const ClientStatus&,
                                             const std::string&)> callback) {
    OnGetFieldValue(selector, callback);
  }

  MOCK_METHOD2(OnGetFieldValue,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)>& callback));

  void SetFieldValue(const Selector& selector,
                     const std::string& value,
                     KeyboardValueFillStrategy fill_strategy,
                     int key_press_delay_in_millisecond,
                     base::OnceCallback<void(const ClientStatus&)> callback) {
    OnSetFieldValue(selector, value, callback);
    OnSetFieldValue(selector, value,
                    fill_strategy == SIMULATE_KEY_PRESSES ||
                        fill_strategy == SIMULATE_KEY_PRESSES_SELECT_VALUE,
                    key_press_delay_in_millisecond, callback);
  }

  MOCK_METHOD3(OnSetFieldValue,
               void(const Selector& selector,
                    const std::string& value,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD5(OnSetFieldValue,
               void(const Selector& selector,
                    const std::string& value,
                    bool simulate_key_presses,
                    int delay_in_millisecond,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD4(SetAttribute,
               void(const Selector& selector,
                    const std::vector<std::string>& attribute,
                    const std::string& value,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  void SendKeyboardInput(
      const Selector& selector,
      const std::vector<UChar32>& codepoints,
      int delay_in_millisecond,
      base::OnceCallback<void(const ClientStatus&)> callback) {
    OnSendKeyboardInput(selector, codepoints, delay_in_millisecond, callback);
  }

  MOCK_METHOD4(OnSendKeyboardInput,
               void(const Selector& selector,
                    const std::vector<UChar32>& codepoints,
                    int delay_in_millisecond,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD2(GetOuterHtml,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));

  MOCK_METHOD2(GetElementTag,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));

  MOCK_METHOD0(ExpectNavigation, void());
  MOCK_METHOD0(ExpectedNavigationHasStarted, bool());
  MOCK_METHOD1(WaitForNavigation,
               bool(base::OnceCallback<void(bool)> callback));
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Restart, void());
  MOCK_CONST_METHOD0(GetUserData, UserData*());
  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_METHOD0(GetWebsiteLoginManager, WebsiteLoginManager*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD0(GetEmailAddressForAccessTokenAccount, std::string());
  MOCK_METHOD0(GetLocale, std::string());
  MOCK_METHOD1(SetDetails, void(std::unique_ptr<Details> details));
  MOCK_METHOD1(SetInfoBox, void(const InfoBox& info_box));
  MOCK_METHOD0(ClearInfoBox, void());
  MOCK_METHOD1(SetProgress, void(int progress));
  MOCK_METHOD1(SetProgressVisible, void(bool visible));
  MOCK_METHOD1(SetUserActions,
               void(std::unique_ptr<std::vector<UserAction>> user_action));
  MOCK_METHOD1(SetViewportMode, void(ViewportMode mode));
  MOCK_METHOD0(GetViewportMode, ViewportMode());
  MOCK_METHOD1(SetPeekMode,
               void(ConfigureBottomSheetProto::PeekMode peek_mode));
  MOCK_METHOD0(GetPeekMode, ConfigureBottomSheetProto::PeekMode());
  MOCK_METHOD0(ExpandBottomSheet, void());
  MOCK_METHOD0(CollapseBottomSheet, void());
  MOCK_METHOD3(
      SetForm,
      bool(std::unique_ptr<FormProto> form,
           base::RepeatingCallback<void(const FormProto::Result*)>
               changed_callback,
           base::OnceCallback<void(const ClientStatus&)> cancel_callback));
  MOCK_METHOD0(GetUserModel, UserModel*());
  MOCK_METHOD0(GetEventHandler, EventHandler*());

  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnWaitForWindowHeightChange(callback);
  }

  MOCK_METHOD1(OnWaitForWindowHeightChange,
               void(base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD2(
      OnGetDocumentReadyState,
      void(const Selector&,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&));

  void GetDocumentReadyState(
      const Selector& frame,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) override {
    OnGetDocumentReadyState(frame, callback);
  }

  MOCK_METHOD3(
      OnWaitForDocumentReadyState,
      void(const Selector&,
           DocumentReadyState min_ready_state,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&));

  void WaitForDocumentReadyState(
      const Selector& frame,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) override {
    OnWaitForDocumentReadyState(frame, min_ready_state, callback);
  }

  MOCK_METHOD0(RequireUI, void());
  MOCK_METHOD0(SetExpandSheetForPromptAction, bool());

  MOCK_METHOD2(
      OnSetGenericUi,
      void(std::unique_ptr<GenericUserInterfaceProto> generic_ui,
           base::OnceCallback<void(bool,
                                   ProcessedActionStatusProto,
                                   const UserModel*)>& end_action_callback));

  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(bool,
                              ProcessedActionStatusProto,
                              const UserModel*)> end_action_callback) override {
    OnSetGenericUi(std::move(generic_ui), end_action_callback);
  }
  MOCK_METHOD0(ClearGenericUi, void());

  const ClientSettings& GetSettings() override { return client_settings_; }

  ClientSettings client_settings_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
