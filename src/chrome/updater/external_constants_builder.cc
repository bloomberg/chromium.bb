// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_builder.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "components/crx_file/crx_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {

namespace {

std::vector<std::string> StringVectorFromGURLVector(
    const std::vector<GURL>& gurls) {
  std::vector<std::string> ret;
  ret.reserve(gurls.size());

  std::transform(gurls.begin(), gurls.end(), std::back_inserter(ret),
                 [](const GURL& gurl) { return gurl.possibly_invalid_spec(); });

  return ret;
}

}  // namespace

ExternalConstantsBuilder::~ExternalConstantsBuilder() {
  LOG_IF(WARNING, !written_) << "An ExternalConstantsBuilder with "
                             << overrides_.size() << " entries is being "
                             << "discarded without being written to a file.";
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetUpdateURL(
    const std::vector<std::string>& urls) {
  base::Value::List url_list;
  url_list.reserve(urls.size());
  for (const std::string& url_string : urls) {
    url_list.Append(url_string);
  }
  overrides_.Set(kDevOverrideKeyUrl, std::move(url_list));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearUpdateURL() {
  overrides_.Remove(kDevOverrideKeyUrl);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetUseCUP(bool use_cup) {
  overrides_.Set(kDevOverrideKeyUseCUP, use_cup);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearUseCUP() {
  overrides_.Remove(kDevOverrideKeyUseCUP);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetInitialDelay(
    double initial_delay) {
  overrides_.Set(kDevOverrideKeyInitialDelay, initial_delay);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearInitialDelay() {
  overrides_.Remove(kDevOverrideKeyInitialDelay);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetServerKeepAliveSeconds(
    int server_keep_alive_seconds) {
  overrides_.Set(kDevOverrideKeyServerKeepAliveSeconds,
                 server_keep_alive_seconds);
  return *this;
}

ExternalConstantsBuilder&
ExternalConstantsBuilder::ClearServerKeepAliveSeconds() {
  overrides_.Remove(kDevOverrideKeyServerKeepAliveSeconds);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetCrxVerifierFormat(
    crx_file::VerifierFormat crx_verifier_format) {
  overrides_.Set(kDevOverrideKeyCrxVerifierFormat,
                 static_cast<int>(crx_verifier_format));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearCrxVerifierFormat() {
  overrides_.Remove(kDevOverrideKeyCrxVerifierFormat);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetGroupPolicies(
    const base::Value::Dict& group_policies) {
  overrides_.Set(kDevOverrideKeyGroupPolicies, group_policies.Clone());
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearGroupPolicies() {
  overrides_.Remove(kDevOverrideKeyGroupPolicies);
  return *this;
}

bool ExternalConstantsBuilder::Overwrite() {
  const absl::optional<base::FilePath> base_path =
      GetBaseDirectory(GetUpdaterScope());
  if (!base_path) {
    LOG(ERROR) << "Can't find base directory; can't save constant overrides.";
    return false;
  }

  const base::FilePath override_file_path =
      base_path.value().AppendASCII(kDevOverrideFileName);
  bool ok = JSONFileValueSerializer(override_file_path).Serialize(overrides_);
  written_ = written_ || ok;
  return ok;
}

bool ExternalConstantsBuilder::Modify() {
  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());
  if (!verifier)
    return Overwrite();

  if (!overrides_.contains(kDevOverrideKeyUrl))
    SetUpdateURL(StringVectorFromGURLVector(verifier->UpdateURL()));
  if (!overrides_.contains(kDevOverrideKeyUseCUP))
    SetUseCUP(verifier->UseCUP());
  if (!overrides_.contains(kDevOverrideKeyInitialDelay))
    SetInitialDelay(verifier->InitialDelay());
  if (!overrides_.contains(kDevOverrideKeyServerKeepAliveSeconds))
    SetServerKeepAliveSeconds(verifier->ServerKeepAliveSeconds());
  if (!overrides_.contains(kDevOverrideKeyCrxVerifierFormat))
    SetCrxVerifierFormat(verifier->CrxVerifierFormat());
  if (!overrides_.contains(kDevOverrideKeyGroupPolicies))
    SetGroupPolicies(verifier->GroupPolicies());

  return Overwrite();
}

}  // namespace updater
