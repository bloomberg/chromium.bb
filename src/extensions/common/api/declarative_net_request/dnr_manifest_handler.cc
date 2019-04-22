// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_handler.h"

#include "base/files/file_path.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace declarative_net_request {

DNRManifestHandler::DNRManifestHandler() = default;
DNRManifestHandler::~DNRManifestHandler() = default;

bool DNRManifestHandler::Parse(Extension* extension, base::string16* error) {
  DCHECK(extension->manifest()->HasKey(keys::kDeclarativeNetRequestKey));
  DCHECK(IsAPIAvailable());

  if (!PermissionsParser::HasAPIPermission(
          extension, APIPermission::kDeclarativeNetRequest)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kDeclarativeNetRequestPermissionNeeded, kAPIPermission,
        keys::kDeclarativeNetRequestKey);
    return false;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kDeclarativeNetRequestKey,
                                            &dict)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeNetRequestKey,
        keys::kDeclarativeNetRequestKey);
    return false;
  }

  // Only a single rules file is supported currently.
  const base::ListValue* rules_file_list = nullptr;
  std::string json_ruleset_location;
  if (!dict->GetList(keys::kDeclarativeRuleResourcesKey, &rules_file_list) ||
      rules_file_list->GetSize() != 1u ||
      !rules_file_list->GetString(0, &json_ruleset_location)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeRulesFileKey,
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey);
    return false;
  }

  ExtensionResource resource = extension->GetResource(json_ruleset_location);
  if (resource.empty() || resource.relative_path().ReferencesParent()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
        keys::kDeclarativeRuleResourcesKey);
    return false;
  }

  extension->SetManifestData(
      keys::kDeclarativeNetRequestKey,
      std::make_unique<DNRManifestData>(
          resource.relative_path().NormalizePathSeparators()));
  return true;
}

bool DNRManifestHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  DCHECK(IsAPIAvailable());

  DNRManifestData* data = static_cast<DNRManifestData*>(
      extension->GetManifestData(manifest_keys::kDeclarativeNetRequestKey));
  DCHECK(data);

  // Check file path validity. We don't use Extension::GetResource since it
  // returns a failure if the relative path contains Windows path separators and
  // we have already normalized the path separators.
  if (!ExtensionResource::GetFilePath(
           extension->path(), data->ruleset_relative_path,
           ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT)
           .empty()) {
    return true;
  }

  *error = ErrorUtils::FormatErrorMessage(errors::kRulesFileIsInvalid,
                                          keys::kDeclarativeNetRequestKey,
                                          keys::kDeclarativeRuleResourcesKey);
  return false;
}

base::span<const char* const> DNRManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDeclarativeNetRequestKey};
  return kKeys;
}

}  // namespace declarative_net_request
}  // namespace extensions
