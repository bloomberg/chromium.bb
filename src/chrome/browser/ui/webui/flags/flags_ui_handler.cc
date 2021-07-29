// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags/flags_ui_handler.h"

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/channel_info.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/version_info/channel.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#endif

FlagsUIHandler::FlagsUIHandler()
    : access_(flags_ui::kGeneralAccessFlagsOnly),
      experimental_features_callback_id_(""),
      deprecated_features_only_(false) {}

FlagsUIHandler::~FlagsUIHandler() {}

void FlagsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      flags_ui::kRequestExperimentalFeatures,
      base::BindRepeating(&FlagsUIHandler::HandleRequestExperimentalFeatures,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kEnableExperimentalFeature,
      base::BindRepeating(
          &FlagsUIHandler::HandleEnableExperimentalFeatureMessage,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kSetOriginListFlag,
      base::BindRepeating(&FlagsUIHandler::HandleSetOriginListFlagMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kRestartBrowser,
      base::BindRepeating(&FlagsUIHandler::HandleRestartBrowser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kResetAllFlags,
      base::BindRepeating(&FlagsUIHandler::HandleResetAllFlags,
                          base::Unretained(this)));
}

void FlagsUIHandler::Init(flags_ui::FlagsStorage* flags_storage,
                          flags_ui::FlagAccess access) {
  flags_storage_.reset(flags_storage);
  access_ = access;

  if (!experimental_features_callback_id_.empty())
    SendExperimentalFeatures();
}

void FlagsUIHandler::HandleRequestExperimentalFeatures(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  experimental_features_callback_id_ = callback_id.GetString();
  // Bail out if the handler hasn't been initialized yet. The request will be
  // handled after the initialization.
  if (!flags_storage_) {
    return;
  }

  SendExperimentalFeatures();
}

void FlagsUIHandler::SendExperimentalFeatures() {
  base::DictionaryValue results;

  base::Value::ListStorage supported_features;
  base::Value::ListStorage unsupported_features;

  if (deprecated_features_only_) {
    about_flags::GetFlagFeatureEntriesForDeprecatedPage(
        flags_storage_.get(), access_, supported_features,
        unsupported_features);
  } else {
    about_flags::GetFlagFeatureEntries(flags_storage_.get(), access_,
                                       supported_features,
                                       unsupported_features);
  }

  results.SetKey(flags_ui::kSupportedFeatures, base::Value(supported_features));
  results.SetKey(flags_ui::kUnsupportedFeatures,
                 base::Value(unsupported_features));
  results.SetBoolean(flags_ui::kNeedsRestart,
                     about_flags::IsRestartNeededToCommitChanges());
  results.SetBoolean(flags_ui::kShowOwnerWarning,
                     access_ == flags_ui::kGeneralAccessFlagsOnly);

#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  version_info::Channel channel = chrome::GetChannel();
  results.SetBoolean(
      flags_ui::kShowBetaChannelPromotion,
      channel == version_info::Channel::STABLE && !deprecated_features_only_);
  results.SetBoolean(
      flags_ui::kShowDevChannelPromotion,
      channel == version_info::Channel::BETA && !deprecated_features_only_);
#else
  results.SetBoolean(flags_ui::kShowBetaChannelPromotion, false);
  results.SetBoolean(flags_ui::kShowDevChannelPromotion, false);
#endif
  ResolveJavascriptCallback(base::Value(experimental_features_callback_id_),
                            results);
  experimental_features_callback_id_.clear();
}

void FlagsUIHandler::HandleEnableExperimentalFeatureMessage(
    const base::ListValue* args) {
  DCHECK(flags_storage_);
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string entry_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &entry_internal_name) ||
      !args->GetString(1, &enable_str) || entry_internal_name.empty()) {
    NOTREACHED();
    return;
  }

  about_flags::SetFeatureEntryEnabled(flags_storage_.get(), entry_internal_name,
                                      enable_str == "true");
}

void FlagsUIHandler::HandleSetOriginListFlagMessage(
    const base::ListValue* args) {
  DCHECK(flags_storage_);
  if (args->GetSize() != 2) {
    NOTREACHED();
    return;
  }

  std::string entry_internal_name;
  std::string value_str;
  if (!args->GetString(0, &entry_internal_name) ||
      !args->GetString(1, &value_str) || entry_internal_name.empty()) {
    NOTREACHED();
    return;
  }

  about_flags::SetOriginListFlag(entry_internal_name, value_str,
                                 flags_storage_.get());
}

void FlagsUIHandler::HandleRestartBrowser(const base::ListValue* args) {
  DCHECK(flags_storage_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS be less intrusive and restart inside the user session after
  // we apply the newly selected flags.
  VLOG(1) << "Restarting to apply per-session flags...";
  ash::about_flags::FeatureFlagsUpdate(*flags_storage_,
                                       Profile::FromWebUI(web_ui())->GetPrefs())
      .UpdateSessionManager();
#endif
  chrome::AttemptRestart();
}

void FlagsUIHandler::HandleResetAllFlags(const base::ListValue* args) {
  DCHECK(flags_storage_);
  about_flags::ResetAllFlags(flags_storage_.get());
}
