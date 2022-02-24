// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_COLLECT_USER_DATA_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_COLLECT_USER_DATA_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
class UserModel;

// Shows a UI to collect user data required for subsequent actions.
class CollectUserDataAction : public Action,
                              public autofill::PersonalDataManagerObserver {
 public:
  explicit CollectUserDataAction(ActionDelegate* delegate,
                                 const ActionProto& proto);

  CollectUserDataAction(const CollectUserDataAction&) = delete;
  CollectUserDataAction& operator=(const CollectUserDataAction&) = delete;

  ~CollectUserDataAction() override;

  // Overrides Action:
  bool ShouldInterruptOnPause() const override;

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

  static bool IsUserDataComplete(
      const UserData& user_data,
      const UserModel& user_model,
      const CollectUserDataOptions& collect_user_data_options);

 private:
  struct LoginDetails {
    LoginDetails(bool choose_automatically_if_no_stored_login,
                 const std::string& payload,
                 const std::string& tag);
    LoginDetails(bool choose_automatically_if_no_stored_login,
                 const std::string& payload,
                 const std::string& tag,
                 const WebsiteLoginManager::Login& login);
    ~LoginDetails();
    bool choose_automatically_if_no_stored_login;
    std::string payload;
    std::string tag;
    // Only for Chrome PWM login details.
    absl::optional<WebsiteLoginManager::Login> login;
  };

  void InternalProcessAction(ProcessActionCallback callback) override;
  void EndAction(const ClientStatus& status);
  bool HasActionEnded() const;

  void OnGetUserData(const CollectUserDataProto& collect_user_data,
                     UserData* user_data,
                     const UserModel* user_model);
  void OnAdditionalActionTriggered(int index,
                                   UserData* user_data,
                                   const UserModel* user_model);
  void OnTermsAndConditionsLinkClicked(int link,
                                       UserData* user_data,
                                       const UserModel* user_model);
  void ReloadAction(UserData* user_data);

  // Only used for logging purposes.
  void OnSelectionStateChanged(UserDataEventField field,
                               UserDataEventType event_type);

  void OnGetLogins(const LoginDetailsProto::LoginOptionProto& login_option,
                   std::vector<WebsiteLoginManager::Login> logins);
  void ShowToUser();
  void OnShowToUser(UserData* user_data, UserData::FieldChange* field_change);
  void UpdateMetrics(UserData* user_data);

  // Creates a new instance of |CollectUserDataOptions| from |proto_|.
  bool CreateOptionsFromProto();

  void FillInitialDataStateForMetrics(
      const std::vector<std::unique_ptr<Contact>>& contacts,
      const std::vector<std::unique_ptr<Address>>& addresses,
      const std::vector<std::unique_ptr<PaymentInstrument>>&
          payment_instruments);

  void FillInitiallySelectedDataStateForMetrics(UserData* user_data);

  void WriteProcessedAction(UserData* user_data, const UserModel* user_model);
  void UpdateProfileAndCardUse(UserData* user_data);

  void UpdateUserDataFromProto(
      const CollectUserDataProto::UserDataProto& proto_data,
      UserData* user_data);
  // Update user data with the new state from personal data manager.
  void UpdatePersonalDataManagerProfiles(
      UserData* user_data,
      UserData::FieldChange* field_change = nullptr);
  void UpdatePersonalDataManagerCards(
      UserData* user_data,
      UserData::FieldChange* field_change = nullptr);
  void UpdateSelectedContact(UserData* user_data);
  void UpdateSelectedPhoneNumber(UserData* user_data);
  void UpdateSelectedShippingAddress(UserData* user_data);
  void UpdateSelectedCreditCard(UserData* user_data);
  void MaybeLogMetrics();

  UserDataMetrics metrics_data_;
  bool shown_to_user_ = false;
  std::unique_ptr<CollectUserDataOptions> collect_user_data_options_;
  ProcessActionCallback callback_;

  // Maps login choice identifiers to the corresponding login details.
  base::flat_map<std::string, std::unique_ptr<LoginDetails>> login_details_map_;

  base::WeakPtrFactory<CollectUserDataAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_COLLECT_USER_DATA_ACTION_H_
