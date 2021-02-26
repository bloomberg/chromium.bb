// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "base/json/json_reader.h"

namespace enterprise_connectors {

namespace {

// Keys used to read service provider config values.
constexpr char kKeyVersion[] = "version";
constexpr char kKeyServiceProviders[] = "service_providers";
constexpr char kKeyName[] = "name";
constexpr char kKeyAnalysis[] = "analysis";
constexpr char kKeyReporting[] = "reporting";
constexpr char kKeyUrl[] = "url";
constexpr char kKeySupportedTags[] = "supported_tags";
constexpr char kKeyMimeTypes[] = "mime_types";
constexpr char kKeyMaxFileSize[] = "max_file_size";

// There is currently only 1 version of this config, so we can just treat it as
// any other value in the JSON with its own key. Once that is no longer the
// case extra code should be added to decide on the correct version to read.
constexpr char kKeyV1[] = "1";

}  // namespace

ServiceProviderConfig::ServiceProviderConfig(const std::string& config) {
  auto config_value = base::JSONReader::Read(config);
  if (!config_value.has_value() || !config_value.value().is_dict())
    return;

  const base::Value* service_providers =
      config_value.value().FindListKey(kKeyServiceProviders);
  if (!service_providers)
    return;

  for (const base::Value& service_provider_value :
       service_providers->GetList()) {
    const std::string* name = service_provider_value.FindStringKey(kKeyName);
    if (name)
      service_providers_.emplace(*name, service_provider_value);
  }
}

const ServiceProviderConfig::ServiceProvider*
ServiceProviderConfig::GetServiceProvider(
    const std::string& service_provider) const {
  if (service_providers_.count(service_provider) == 0)
    return nullptr;
  return &service_providers_.at(service_provider);
}

ServiceProviderConfig::ServiceProviderConfig(ServiceProviderConfig&&) = default;
ServiceProviderConfig::~ServiceProviderConfig() = default;

ServiceProviderConfig::ServiceProvider::ServiceProvider(
    const base::Value& config) {
  if (!config.is_dict())
    return;

  const base::Value* versions = config.FindDictKey(kKeyVersion);
  if (!versions || !versions->is_dict())
    return;

  const base::Value* version_1 = versions->FindDictKey(kKeyV1);
  if (!version_1 || !version_1->is_dict())
    return;

  const base::Value* analysis = version_1->FindDictKey(kKeyAnalysis);
  if (analysis && analysis->is_dict()) {
    const std::string* analysis_url = analysis->FindStringKey(kKeyUrl);
    if (analysis_url)
      analysis_url_ = *analysis_url;

    const base::Value* supported_tags =
        analysis->FindListKey(kKeySupportedTags);
    if (supported_tags) {
      for (const base::Value& tag : supported_tags->GetList()) {
        if (!tag.is_dict())
          continue;

        const std::string* name = tag.FindStringKey(kKeyName);
        if (name)
          analysis_tags_.emplace(*name, tag);
      }
    }
  }

  const base::Value* reporting = version_1->FindDictKey(kKeyReporting);
  if (reporting) {
    const std::string* reporting_url = reporting->FindStringKey(kKeyUrl);
    if (reporting_url)
      reporting_url_ = *reporting_url;
  }
}

ServiceProviderConfig::ServiceProvider::ServiceProvider(ServiceProvider&&) =
    default;
ServiceProviderConfig::ServiceProvider::~ServiceProvider() = default;

ServiceProviderConfig::ServiceProvider::Tag::Tag(const base::Value& tag_value) {
  if (!tag_value.is_dict())
    return;

  const base::Value* mime_types = tag_value.FindListKey(kKeyMimeTypes);
  if (mime_types) {
    for (const base::Value& mime_type : mime_types->GetList()) {
      if (!mime_type.is_string())
        continue;

      mime_types_.push_back(mime_type.GetString());
    }
  }

  max_file_size_ = tag_value.FindIntKey(kKeyMaxFileSize).value_or(-1);
}

ServiceProviderConfig::ServiceProvider::Tag::Tag(Tag&&) = default;
ServiceProviderConfig::ServiceProvider::Tag::~Tag() = default;

}  // namespace enterprise_connectors
