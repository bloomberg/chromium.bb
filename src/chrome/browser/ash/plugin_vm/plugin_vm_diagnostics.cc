// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_diagnostics.h"

#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics_builder.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "components/prefs/pref_service.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "ui/base/l10n/l10n_util.h"

namespace plugin_vm {

namespace {

using DiagnosticsCallback =
    base::OnceCallback<void(guest_os::mojom::DiagnosticsPtr)>;

std::string CapitalizedBoardName() {
  const std::string uppercase = base::SysInfo::HardwareModelName();

  CHECK_GE(uppercase.size(), 1);
  base::StringPiece uppercase_first_char(uppercase.c_str(), 1);
  base::StringPiece uppercase_remaining(uppercase.c_str() + 1,
                                        uppercase.length() - 1);

  return base::StrCat(
      {uppercase_first_char, base::ToLowerASCII(uppercase_remaining)});
}

class PluginVmDiagnostics : public base::RefCounted<PluginVmDiagnostics> {
 public:
  static void Run(DiagnosticsCallback callback) {
    // Kick off the first step. The object is kept alive in callbacks until it
    // is finished.
    base::WrapRefCounted(new PluginVmDiagnostics(std::move(callback)))
        ->CheckPluginVmIsAllowed();
  }

 private:
  friend class base::RefCounted<PluginVmDiagnostics>;

  using EntryBuilder = guest_os::DiagnosticsBuilder::EntryBuilder;
  using ImageListType =
      ::google::protobuf::RepeatedPtrField<vm_tools::concierge::VmDiskInfo>;

  explicit PluginVmDiagnostics(DiagnosticsCallback callback)
      : active_profile_{ProfileManager::GetActiveUserProfile()},
        callback_(std::move(callback)) {}
  ~PluginVmDiagnostics() { DCHECK(callback_.is_null()); }

  void CheckPluginVmIsAllowed() {
    using ProfileSupported = plugin_vm::PluginVmFeatures::ProfileSupported;
    using PolicyConfigured = plugin_vm::PluginVmFeatures::PolicyConfigured;

    auto is_allowed_diagnostics =
        plugin_vm::PluginVmFeatures::Get()->GetIsAllowedDiagnostics(
            active_profile_);

    // TODO(b/173653141): Consider reconciling the error messages with
    // `is_allowed_diagnostics.GetTopError()` so that we can reuse it.

    {
      EntryBuilder entry(IDS_VM_STATUS_PAGE_DEVICE_IS_SUPPORTED_REQUIREMENT);
      if (!is_allowed_diagnostics.device_supported) {
        entry.SetFail(l10n_util::GetStringFUTF8(
            IDS_VM_STATUS_PAGE_DEVICE_IS_SUPPORTED_EXPLANATION,
            base::UTF8ToUTF16(CapitalizedBoardName())));
      }
      builder_.AddEntry(std::move(entry));
    }

    {
      EntryBuilder entry(IDS_VM_STATUS_PAGE_PROFILE_IS_SUPPORTED_REQUIREMENT);

      switch (is_allowed_diagnostics.profile_supported) {
        case ProfileSupported::kOk:
          break;
        case ProfileSupported::kErrorNonPrimary:
          entry.SetFail(IDS_VM_STATUS_PAGE_SECONDARY_PROFILE_EXPLANATION);
          break;
        case ProfileSupported::kErrorChildAccount:
          entry.SetFail(IDS_VM_STATUS_PAGE_CHILD_PROFILE_EXPLANATION);
          break;
        case ProfileSupported::kErrorOffTheRecord:
          entry.SetFail(IDS_VM_STATUS_PAGE_GUEST_PROFILE_EXPLANATION);
          break;
        case ProfileSupported::kErrorEphemeral:
          entry.SetFail(
              IDS_VM_STATUS_PAGE_EPHEMERAL_PROFILE_EXPLANATION,
              /*learn_more_link=*/
              GURL("https://support.google.com/chromebook?p=ephemeral_mode"));
          break;
        case ProfileSupported::kErrorNotSupported:
          entry.SetFail(IDS_VM_STATUS_PAGE_UNSUPPORTED_PROFILE_EXPLANATION);
          break;
      }
      builder_.AddEntry(std::move(entry));
    }

    {
      EntryBuilder entry(IDS_VM_STATUS_PAGE_POLICIES_REQUIREMENT);
      bool set_standard_top_error = true;

      switch (is_allowed_diagnostics.policy_configured) {
        case PolicyConfigured::kOk: {
          // Additional check for image policy. See b/185281662#comment2.
          const base::DictionaryValue* image_policy =
              active_profile_->GetPrefs()->GetDictionary(prefs::kPluginVmImage);
          const base::Value* url =
              image_policy->FindKey(prefs::kPluginVmImageUrlKeyName);
          const base::Value* hash =
              image_policy->FindKey(prefs::kPluginVmImageHashKeyName);
          if (!url || !GURL(url->GetString()).is_valid()) {
            entry.SetFail(IDS_VM_STATUS_PAGE_IMAGE_URL_POLICY_EXPLANATION);
          } else if (!hash || hash->GetString().empty()) {
            entry.SetFail(IDS_VM_STATUS_PAGE_IMAGE_HASH_POLICY_EXPLANATION);
          } else {
            set_standard_top_error = false;
          }
        } break;
        case PolicyConfigured::kErrorUnableToCheckPolicy:
          entry.SetFail(
              IDS_VM_STATUS_PAGE_UNABLE_TO_CHECK_POLICIES_EXPLANATION);
          break;
        case PolicyConfigured::kErrorNotEnterpriseEnrolled:
          entry.SetFail(IDS_VM_STATUS_PAGE_DEVICE_NOT_ENROLLED_EXPLANATION)
              .OverrideTopError(
                  IDS_VM_STATUS_PAGE_DEVICE_NOT_ENROLLED_ERROR,
                  /*learn_more_link=*/GURL(
                      "https://support.google.com/chromebook?p=enroll_device"));
          set_standard_top_error = false;
          break;
        case PolicyConfigured::kErrorUserNotAffiliated:
          entry.SetFail(IDS_VM_STATUS_PAGE_USER_NOT_AFFILIATED_EXPLANATION);
          break;
        case PolicyConfigured::kErrorUnableToCheckDevicePolicy:
          entry.SetFail(
              IDS_VM_STATUS_PAGE_UNABLE_TO_CHECK_DEVICE_ALLOW_EXPLANATION);
          break;
        case PolicyConfigured::kErrorNotAllowedByDevicePolicy:
          entry.SetFail(IDS_VM_STATUS_PAGE_DEVICE_NOT_ALLOW_EXPLANATION);
          break;
        case PolicyConfigured::kErrorNotAllowedByUserPolicy:
          entry.SetFail(IDS_VM_STATUS_PAGE_USER_NOT_ALLOW_EXPLANATION);
          break;
        case PolicyConfigured::kErrorLicenseNotSetUp:
          entry.SetFail(IDS_VM_STATUS_PAGE_LICENSE_NOT_SET_UP_EXPLANATION);
          break;
      }
      if (set_standard_top_error) {
        entry.OverrideTopError(IDS_VM_STATUS_PAGE_INCORRECT_POLICIES_ERROR);
      }
      builder_.AddEntry(std::move(entry));
    }

    // Next step.
    CheckDefaultVmExists(is_allowed_diagnostics.IsOk());
  }

  void CheckDefaultVmExists(bool plugin_vm_is_allowed) {
    if (!plugin_vm_is_allowed) {
      OnListVmDisks(false, absl::nullopt);
      return;
    }

    vm_tools::concierge::ListVmDisksRequest request;
    request.set_cryptohome_id(
        chromeos::ProfileHelper::GetUserIdHashFromProfile(active_profile_));
    request.set_storage_location(
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);

    chromeos::ConciergeClient::Get()->ListVmDisks(
        std::move(request),
        base::BindOnce(&PluginVmDiagnostics::OnListVmDisks, this,
                       /*plugin_vm_is_allowed=*/true));
  }

  void OnListVmDisks(
      bool plugin_vm_is_allowed,
      absl::optional<vm_tools::concierge::ListVmDisksResponse> response) {
    EntryBuilder entry(l10n_util::GetStringFUTF8(
        IDS_VM_STATUS_PAGE_DEFAULT_VM_EXISTS_REQUIREMENT,
        l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));

    if (plugin_vm_is_allowed) {
      if (!response.has_value()) {
        entry.SetFail(IDS_VM_STATUS_PAGE_FAILED_TO_CHECK_VM_EXPLANATION);
      } else if (!HasDefaultVm(response->images())) {
        entry.SetFail(GetMissingDefaultVmExplanation(response->images()))
            .OverrideTopError(
                l10n_util::GetStringFUTF8(
                    IDS_VM_STATUS_PAGE_MISSING_DEFAULT_VM_ERROR,
                    l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)),
                /*learn_more_link=*/
                GURL(
                    "https://support.google.com/chromebook?p=parallels_setup"));
      } else {
        // Everything is good. Do nothing.
      }
    } else {  // !plugin_vm_is_allowed.
      entry.SetNotApplicable();
    }

    builder_.AddEntry(std::move(entry));

    Finish();
  }

  void Finish() {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), builder_.Build()));
  }

  bool HasDefaultVm(const ImageListType& images) {
    for (auto& image : images) {
      if (image.name() == plugin_vm::kPluginVmName) {
        return true;
      }
    }
    return false;
  }

  std::string GetMissingDefaultVmExplanation(const ImageListType& images) {
    std::string string_template = l10n_util::GetPluralStringFUTF8(
        IDS_VM_STATUS_PAGE_MISSING_DEFAULT_VM_EXPLANATION, images.size());
    std::vector<std::string> subs{
        l10n_util::GetStringUTF8(IDS_PLUGIN_VM_APP_NAME)};

    if (images.size() > 0) {
      // In this case, we have a second placeholder VM_NAME_LIST. The substitute
      // should looke like this: `"vm1", "vm2"`. Note that the l10n tooling does
      // not support formatting a list of strings, which is why we have to do
      // the formatting by ourselves here. The formatting might not be ideal for
      // all languages, but it should be good enough for its purpose.
      std::stringstream stream;
      bool first_vm = true;
      for (auto& image : images) {
        if (!first_vm) {
          stream << ", ";
        }
        stream << '"' << image.name() << '"';

        first_vm = false;
      }

      subs.push_back(stream.str());
    }

    return base::ReplaceStringPlaceholders(string_template, subs, nullptr);
  }

  Profile* const active_profile_;
  DiagnosticsCallback callback_;
  guest_os::DiagnosticsBuilder builder_;
};

}  // namespace

void GetDiagnostics(
    base::OnceCallback<void(guest_os::mojom::DiagnosticsPtr)> callback) {
  PluginVmDiagnostics::Run(std::move(callback));
}

}  // namespace plugin_vm
