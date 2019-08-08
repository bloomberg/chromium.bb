// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/enterprise_printers_provider.h"

#include <vector>

#include "base/feature_list.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/printing/calculators_policies_binder.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

std::vector<std::string> ConvertToVector(const base::ListValue* list) {
  std::vector<std::string> string_list;
  if (list) {
    for (const base::Value& value : *list) {
      if (value.is_string()) {
        string_list.push_back(value.GetString());
      }
    }
  }
  return string_list;
}

class EnterprisePrintersProviderImpl : public EnterprisePrintersProvider,
                                       public BulkPrintersCalculator::Observer {
 public:
  EnterprisePrintersProviderImpl(CrosSettings* settings, Profile* profile)
      : profile_(profile) {
    // initialization of pref_change_registrar
    pref_change_registrar_.Init(profile->GetPrefs());

    if (base::FeatureList::IsEnabled(features::kBulkPrinters)) {
      // Binds instances of BulkPrintersCalculator to policies.
      policies_binder_ = CalculatorsPoliciesBinder::Create(settings, profile);
      // Get instance of BulkPrintersCalculator for device policies.
      device_printers_ = BulkPrintersCalculatorFactory::Get()->GetForDevice();
      if (device_printers_) {
        device_printers_->AddObserver(this);
        RecalculateCompleteFlagForDevicePrinters();
      }
      // Get instance of BulkPrintersCalculator for user policies.
      user_printers_ =
          BulkPrintersCalculatorFactory::Get()->GetForProfile(profile);
      if (user_printers_) {
        user_printers_->AddObserver(this);
        RecalculateCompleteFlagForUserPrinters();
      }
    } else {
      // If a "Bulk Printers" feature is inactive, we do not bind anything.
      // The list of printers is always empty and is reported as complete.
      complete_ = true;
    }
    // Binds policy with recommended printers (deprecated). This method calls
    // indirectly RecalculateCurrentPrintersList() that prepares the first
    // version of final list of printers.
    BindPref(prefs::kRecommendedNativePrinters,
             &EnterprisePrintersProviderImpl::UpdateUserRecommendedPrinters);
  }

  ~EnterprisePrintersProviderImpl() override {
    if (device_printers_)
      device_printers_->RemoveObserver(this);
    if (user_printers_)
      user_printers_->RemoveObserver(this);
  }

  void AddObserver(EnterprisePrintersProvider::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.AddObserver(observer);
    observer->OnPrintersChanged(complete_, printers_);
  }

  void RemoveObserver(EnterprisePrintersProvider::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.RemoveObserver(observer);
  }

  // BulkPrintersCalculator::Observer implementation
  void OnPrintersChanged(const BulkPrintersCalculator* sender) override {
    if (sender == device_printers_.get()) {
      RecalculateCompleteFlagForDevicePrinters();
    } else {
      RecalculateCompleteFlagForUserPrinters();
    }
    RecalculateCurrentPrintersList();
  }

 private:
  // This method process value from the deprecated policy with recommended
  // printers. It is called when value of the policy changes.
  void UpdateUserRecommendedPrinters() {
    recommended_printers_.clear();
    std::vector<std::string> data =
        FromPrefs(prefs::kRecommendedNativePrinters);
    for (const auto& printer_json : data) {
      std::unique_ptr<base::DictionaryValue> printer_dictionary =
          base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
              printer_json, base::JSON_ALLOW_TRAILING_COMMAS));
      if (!printer_dictionary) {
        LOG(WARNING) << "Ignoring invalid printer.  Invalid JSON object: "
                     << printer_json;
        continue;
      }

      // Policy printers don't have id's but the ids only need to be locally
      // unique so we'll hash the record.  This will not collide with the
      // UUIDs generated for user entries.
      std::string id = base::MD5String(printer_json);
      printer_dictionary->SetString(kPrinterId, id);

      auto new_printer = RecommendedPrinterToPrinter(*printer_dictionary);
      if (!new_printer) {
        LOG(WARNING) << "Recommended printer is malformed.";
        continue;
      }

      if (!recommended_printers_.insert({id, *new_printer}).second) {
        // Printer is already in the list.
        LOG(WARNING) << "Duplicate printer ignored: " << id;
        continue;
      }
    }
    RecalculateCurrentPrintersList();
  }

  // These three methods calculate resultant list of printers and complete flag.

  void RecalculateCompleteFlagForUserPrinters() {
    user_printers_is_complete_ =
        user_printers_->IsComplete() &&
        (user_printers_->IsDataPolicySet() ||
         !PolicyWithDataIsSet(policy::key::kNativePrintersBulkConfiguration));
  }

  void RecalculateCompleteFlagForDevicePrinters() {
    device_printers_is_complete_ =
        device_printers_->IsComplete() &&
        (device_printers_->IsDataPolicySet() ||
         !PolicyWithDataIsSet(policy::key::kDeviceNativePrinters));
  }

  void RecalculateCurrentPrintersList() {
    complete_ = true;
    printers_ = recommended_printers_;
    if (device_printers_) {
      complete_ = complete_ && device_printers_is_complete_;
      const auto& printers = device_printers_->GetPrinters();
      printers_.insert(printers.begin(), printers.end());
    }
    if (user_printers_) {
      complete_ = complete_ && user_printers_is_complete_;
      const auto& printers = user_printers_->GetPrinters();
      printers_.insert(printers.begin(), printers.end());
    }
    for (auto& observer : observers_) {
      observer.OnPrintersChanged(complete_, printers_);
    }
  }

  typedef void (EnterprisePrintersProviderImpl::*SimpleMethod)();

  // Binds given user policy to given method and calls this method once.
  void BindPref(const char* policy_name, SimpleMethod method_to_call) {
    pref_change_registrar_.Add(
        policy_name,
        base::BindRepeating(method_to_call, base::Unretained(this)));
    (this->*method_to_call)();
  }

  // Extracts the list of strings named |policy_name| from user policies.
  std::vector<std::string> FromPrefs(const std::string& policy_name) {
    return ConvertToVector(profile_->GetPrefs()->GetList(policy_name));
  }

  // Checks if given policy is set and if it is a dictionary
  bool PolicyWithDataIsSet(const char* policy_name) {
    policy::ProfilePolicyConnector* policy_connector =
        profile_->GetProfilePolicyConnector();
    if (!policy_connector) {
      // something is wrong
      return false;
    }
    const policy::PolicyNamespace policy_namespace =
        policy::PolicyNamespace(policy::PolicyDomain::POLICY_DOMAIN_CHROME, "");
    const policy::PolicyMap& policy_map =
        policy_connector->policy_service()->GetPolicies(policy_namespace);
    const base::Value* value = policy_map.GetValue(policy_name);
    if (value && value->is_dict()) {
      // policy is set and its value is a dictionary
      return true;
    }
    return false;
  }

  // current partial results
  std::unordered_map<std::string, Printer> recommended_printers_;
  bool device_printers_is_complete_ = true;
  bool user_printers_is_complete_ = true;

  // current final results
  bool complete_ = false;
  std::unordered_map<std::string, Printer> printers_;

  // Calculators for bulk printers from device and user policies. Unowned.
  base::WeakPtr<BulkPrintersCalculator> device_printers_;
  base::WeakPtr<BulkPrintersCalculator> user_printers_;

  // Policies binder (bridge between policies and calculators). Owned.
  std::unique_ptr<CalculatorsPoliciesBinder> policies_binder_;

  // Profile (user) settings.
  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<EnterprisePrintersProvider::Observer>::Unchecked
      observers_;
  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(EnterprisePrintersProviderImpl);
};

}  // namespace

// static
void EnterprisePrintersProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kRecommendedNativePrinters);
  CalculatorsPoliciesBinder::RegisterProfilePrefs(registry);
}

// static
std::unique_ptr<EnterprisePrintersProvider> EnterprisePrintersProvider::Create(
    CrosSettings* settings,
    Profile* profile) {
  return std::make_unique<EnterprisePrintersProviderImpl>(settings, profile);
}

}  // namespace chromeos
