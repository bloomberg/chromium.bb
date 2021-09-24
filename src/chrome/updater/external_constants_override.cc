// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if defined(OS_MAC)
#include "base/mac/foundation_util.h"
#elif defined(OS_WIN)
#include "base/path_service.h"
#endif

namespace {

std::vector<GURL> GURLVectorFromStringList(
    base::Value::ConstListView update_url_list) {
  std::vector<GURL> ret;
  ret.reserve(update_url_list.size());
  for (const base::Value& url : update_url_list) {
    CHECK(url.is_string()) << "Non-string Value in update URL list";
    ret.push_back(GURL(url.GetString()));
  }
  return ret;
}

}  // anonymous namespace

namespace updater {

ExternalConstantsOverrider::ExternalConstantsOverrider(
    base::flat_map<std::string, base::Value> override_values,
    scoped_refptr<ExternalConstants> next_provider)
    : ExternalConstants(std::move(next_provider)),
      override_values_(std::move(override_values)) {}

ExternalConstantsOverrider::~ExternalConstantsOverrider() = default;

std::vector<GURL> ExternalConstantsOverrider::UpdateURL() const {
  if (!override_values_.contains(kDevOverrideKeyUrl)) {
    return next_provider_->UpdateURL();
  }
  const base::Value& update_url_value = override_values_.at(kDevOverrideKeyUrl);
  switch (update_url_value.type()) {
    case base::Value::Type::STRING:
      return {GURL(update_url_value.GetString())};
    case base::Value::Type::LIST:
      return GURLVectorFromStringList(update_url_value.GetList());
    default:
      LOG(FATAL) << "Unexpected type of override[" << kDevOverrideKeyUrl
                 << "]: " << base::Value::GetTypeName(update_url_value.type());
      NOTREACHED();
  }
  NOTREACHED();
  return {};
}

bool ExternalConstantsOverrider::UseCUP() const {
  if (!override_values_.contains(kDevOverrideKeyUseCUP)) {
    return next_provider_->UseCUP();
  }
  const base::Value& use_cup_value = override_values_.at(kDevOverrideKeyUseCUP);
  CHECK(use_cup_value.is_bool())
      << "Unexpected type of override[" << kDevOverrideKeyUseCUP
      << "]: " << base::Value::GetTypeName(use_cup_value.type());

  return use_cup_value.GetBool();
}

double ExternalConstantsOverrider::InitialDelay() const {
  if (!override_values_.contains(kDevOverrideKeyInitialDelay)) {
    return next_provider_->InitialDelay();
  }

  const base::Value& initial_delay_value =
      override_values_.at(kDevOverrideKeyInitialDelay);
  CHECK(initial_delay_value.is_double())
      << "Unexpected type of override[" << kDevOverrideKeyInitialDelay
      << "]: " << base::Value::GetTypeName(initial_delay_value.type());
  return initial_delay_value.GetDouble();
}

int ExternalConstantsOverrider::ServerKeepAliveSeconds() const {
  if (!override_values_.contains(kDevOverrideKeyServerKeepAliveSeconds)) {
    return next_provider_->ServerKeepAliveSeconds();
  }

  const base::Value& server_keep_alive_seconds_value =
      override_values_.at(kDevOverrideKeyServerKeepAliveSeconds);
  CHECK(server_keep_alive_seconds_value.is_int())
      << "Unexpected type of override[" << kDevOverrideKeyServerKeepAliveSeconds
      << "]: "
      << base::Value::GetTypeName(server_keep_alive_seconds_value.type());
  return server_keep_alive_seconds_value.GetInt();
}

// static
scoped_refptr<ExternalConstantsOverrider>
ExternalConstantsOverrider::FromDefaultJSONFile(
    scoped_refptr<ExternalConstants> next_provider) {
  const absl::optional<base::FilePath> data_dir_path =
      GetBaseDirectory(GetUpdaterScope());
  if (!data_dir_path) {
    LOG(ERROR) << "Cannot find app data path.";
    return nullptr;
  }
  const base::FilePath override_file_path =
      data_dir_path->AppendASCII(kDevOverrideFileName);

  JSONFileValueDeserializer parser(override_file_path,
                                   base::JSON_ALLOW_TRAILING_COMMAS);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> parsed_value(
      parser.Deserialize(&error_code, &error_message));
  if (error_code || !parsed_value) {
    LOG(ERROR) << "Could not parse " << override_file_path << ": error "
               << error_code << ": " << error_message;
    return nullptr;
  }

  if (!parsed_value->is_dict()) {
    LOG(ERROR) << "Invalid data in " << override_file_path << ": not a dict";
    return nullptr;
  }

  return base::MakeRefCounted<ExternalConstantsOverrider>(
      std::move(*parsed_value).TakeDict(), next_provider);
}

// Declared in external_constants.h. This implementation of the function is
// used only if external_constants_override is linked into the binary.
scoped_refptr<ExternalConstants> CreateExternalConstants() {
  scoped_refptr<ExternalConstants> overrider =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());
  return overrider ? overrider : CreateDefaultExternalConstants();
}

}  // namespace updater
