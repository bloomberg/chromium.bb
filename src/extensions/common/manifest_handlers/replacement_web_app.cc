// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/replacement_web_app.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const ReplacementWebAppInfo* GetReplacementWebAppInfo(
    const Extension* extension) {
  return static_cast<ReplacementWebAppInfo*>(
      extension->GetManifestData(keys::kReplacementWebApp));
}

}  // namespace

ReplacementWebAppInfo::ReplacementWebAppInfo() {}

ReplacementWebAppInfo::~ReplacementWebAppInfo() {}

// static
bool ReplacementWebAppInfo::HasReplacementWebApp(const Extension* extension) {
  const ReplacementWebAppInfo* info = GetReplacementWebAppInfo(extension);
  return info;
}

// static
GURL ReplacementWebAppInfo::GetReplacementWebApp(const Extension* extension) {
  const ReplacementWebAppInfo* info = GetReplacementWebAppInfo(extension);
  if (info)
    return info->replacement_web_app;

  return GURL();
}

ReplacementWebAppHandler::ReplacementWebAppHandler() {}

ReplacementWebAppHandler::~ReplacementWebAppHandler() {}

bool ReplacementWebAppHandler::Parse(Extension* extension,
                                     base::string16* error) {
  std::string string_value;
  if (!extension->manifest()->GetString(keys::kReplacementWebApp,
                                        &string_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidReplacementWebApp);
    return false;
  }
  const GURL replacement_web_app(string_value);
  if (!replacement_web_app.is_valid() ||
      !replacement_web_app.SchemeIs(url::kHttpsScheme)) {
    *error = base::ASCIIToUTF16(errors::kInvalidReplacementWebApp);
    return false;
  }
  auto info = std::make_unique<ReplacementWebAppInfo>();
  info->replacement_web_app = std::move(replacement_web_app);
  extension->SetManifestData(keys::kReplacementWebApp, std::move(info));
  return true;
}

base::span<const char* const> ReplacementWebAppHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kReplacementWebApp};
  return kKeys;
}

}  // namespace extensions
